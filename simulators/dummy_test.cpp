#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <fstream>
#include <algorithm> // for std::min
#include <stdexcept> // for std::runtime_error
#include <cstdio>    // for std::remove
#include <thread>    // for multithreading
#include <atomic>

// Windows env for testing
#if defined(_WIN32)
#include <windows.h>
#endif

#include "cmdline.h"                    // for command line parsing
#include "hardware/dvfs.h"              // for DVFS control (reuse)
#include "hardware/record.h" // for hardware recording (reuse)

// type aliases for clarity
using Vector = std::vector<float>;
using Matrix = std::vector<std::vector<float>>;

// --- 1. file I/O and memory access functions ---
void create_dummy_file(const std::string &filename, int size_mb) {
    std::cout << "[I/O] Creating " << size_mb << " MB size of dummy model file..." << std::endl;
    std::ofstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("ERROR: Could not create the model: " + filename);

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<short> dis(0, 255);

    long long bytes_to_write = static_cast<long long>(size_mb) * 1024 * 1024;
    const size_t buffer_size = 4096;
    std::vector<char> buffer(buffer_size);

    while (bytes_to_write > 0) {
        for (size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] = static_cast<char>(dis(gen));
        }
        long long current_write_size = std::min(static_cast<long long>(buffer_size), bytes_to_write);
        file.write(buffer.data(), current_write_size);
        bytes_to_write -= current_write_size;
    }
    file.close();
}

Matrix initialize_matrix_from_file(int rows, int cols, std::ifstream &file) {
    Matrix mat(rows, Vector(cols));
    for (int i = 0; i < rows; ++i) {
        if (!file.read(reinterpret_cast<char *>(mat[i].data()), cols * sizeof(float))) {
            throw std::runtime_error("ERROR: Failed to read matrix data from the file.");
        }
    }
    return mat;
}

// virtual read functions to simulate memory access
const Matrix &read(const Matrix &mat) {
    return mat;
}
const Vector &read(const Vector &vec) {
    return vec;
}

// --- 2. GEMM, GEMV, and Transformer layer simulation functions ---
Matrix gemm(const Matrix &A, const Matrix &B, const std::string &op_name = "", const int &num_th = 4, const bool verbose = false) {
    // debugging output
    if (!op_name.empty() && verbose) {
        std::cout << "\n[GEMM Debug] Operation: '" << op_name << "'" << std::endl;
        if (!A.empty() && !A[0].empty())
            std::cout << "  - Matrix A dims: (" << A.size() << ", " << A[0].size() << ")" << std::endl;
        else
            std::cout << "  - Matrix A is empty or malformed." << std::endl;
        if (!B.empty() && !B[0].empty())
            std::cout << "  - Matrix B dims: (" << B.size() << ", " << B[0].size() << ")" << std::endl;
        else
            std::cout << "  - Matrix B is empty or malformed." << std::endl;
    }

    if (A.empty() || B.empty() || A[0].size() != B.size()) {
        throw std::invalid_argument("Invalid GEMM dimensions.");
    }

    int m = A.size(), k = B.size(), n = B[0].size();
    Matrix C(m, Vector(n, 0.0f));

    unsigned int num_threads = num_th;
    if (num_threads == 0) num_threads = 4;
    std::vector<std::thread> threads;

    int rows_per_thread = m / num_threads;

    // worker function for each thread
    auto worker = [&](int start_row, int end_row) {
        for (int i = start_row; i < end_row; ++i) {
            for (int j = 0; j < n; ++j) {
                for (int l = 0; l < k; ++l) {
                    C[i][j] += A[i][l] * B[l][j];
                }
            }
        }
    };

    // spawn threads and distribute work
    for (unsigned int i = 0; i < num_threads; ++i) {
        int start_row = i * rows_per_thread;
        int end_row = (i == num_threads - 1) ? m : start_row + rows_per_thread;
        threads.emplace_back(worker, start_row, end_row);
    }

    // wait for all threads to finish
    for (auto &th : threads) { th.join(); }

    return C;
}

Vector gemv(const Vector &y, const Matrix &A, const Vector &x, const int &num_th = 4, const bool verbose = false) {
    if (verbose) { /* empty */
    }

    if (A.empty() || x.empty() || A[0].size() != x.size()) throw std::invalid_argument("Invalid GEMV dimensions.");
    int m = A.size(), n = x.size();
    Vector result_y = y; // y copy

    unsigned int num_threads = num_th;
    if (num_threads == 0) num_threads = 4;
    std::vector<std::thread> threads;

    int rows_per_thread = m / num_threads;

    // worker function for each thread
    auto worker = [&](int start_row, int end_row) {
        for (int i = start_row; i < end_row; ++i) {
            for (int j = 0; j < n; ++j) {
                result_y[i] += A[i][j] * x[j];
            }
        }
    };

    // spawn threads and distribute work
    for (unsigned int i = 0; i < num_threads; ++i) {
        int start_row = i * rows_per_thread;
        int end_row = (i == num_threads - 1) ? m : start_row + rows_per_thread;
        threads.emplace_back(worker, start_row, end_row);
    }

    // wait for all threads to finish
    for (auto &th : threads) { th.join(); }

    return result_y;
}

// [mod] add name tags for debugging in GEMM
Matrix transformer_layer_prefill(const Matrix &input,
                                 const Matrix &W_q, const Matrix &W_k, const Matrix &W_v,
                                 const Matrix &W_o, const Matrix &W_ffn1, const Matrix &W_ffn2,
                                 const int &num_th) {
    // prefill: input shape (seq_len, hidden_dim)
    Matrix Q = gemm(input, W_q, "Prefill: Q = input * W_q", num_th);
    Matrix AttentionOutput = gemm(Q, W_v, "Prefill: AttentionOutput = Q * W_v", num_th);
    Matrix AttentionFinal = gemm(AttentionOutput, W_o, "Prefill: AttentionFinal = AttentionOutput * W_o", num_th);
    Matrix ffn1_output = gemm(AttentionFinal, W_ffn1, "Prefill: ffn1_output = AttentionFinal * W_ffn1", num_th);
    Matrix ffn2_output = gemm(ffn1_output, W_ffn2, "Prefill: ffn2_output = ffn1_output * W_ffn2", num_th);
    return ffn2_output;
}

Vector transformer_layer_decode(const Vector &token,
                                const Matrix &W_q, const Matrix &W_k, const Matrix &W_v,
                                const Matrix &W_o, const Matrix &W_ffn1, const Matrix &W_ffn2,
                                const int &num_th) {
    // decode: token shape (hidden_dim,)
    Vector y(W_q.size(), 0.0f);
    Vector q = gemv(y, W_q, token, num_th);
    Vector v = gemv(y, W_v, token, num_th);
    Vector AttentionOutput = gemv(y, W_o, v, num_th);

    Vector y_ffn(W_ffn2.size(), 0.0f);
    Vector ffn1_output = gemv(y_ffn, W_ffn2, AttentionOutput, num_th);

    Vector y_final(W_ffn1.size(), 0.0f);
    Vector ffn2_output = gemv(y_final, W_ffn1, ffn1_output, num_th);
    return ffn2_output;
}

// --- 3. main function ---
std::atomic_bool sigterm(false);

int main(int argc, char **argv) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::iostream::sync_with_stdio(false);
    // cmdline::parser cmdParser;

    // TODO: adjust hidden_dim and ffn_size value
    // arg parser
    // cmdParser.add<std::string>("model", 'm', "a filename of dummy model", false, "dummy_model_weights.bin");
    // cmdParser.add<int>("num_queries", 'q', "the number of inference", false, 10);
    // cmdParser.add<int>("num_layers", 'l', "the number of layers", false, 24);
    // cmdParser.add<int>("hidden_dim", 'h', "hidden dimension", false, 256); // lowered by fp16 memory issue
    // cmdParser.add<int>("ffn_size", 'f', "hidden dimension", false, 588);   // lowered by fp16 memory issue
    // cmdParser.add<int>("input_tokens", 'i', "input length (alias: prompt tokens)", false, 64);
    // cmdParser.add<int>("output_tokens", 'o', "output length (alias: generation tokens)", false, 256);
    // cmdParser.add<int>("num_threads", 't', "number of threads", false, 4);
    // cmdParser.add<int>("cpu_clock", 'c', "number of threads", true, 12);
    // cmdParser.add<int>("ram_clock", 'r', "number of threads", true, 11);
    // cmdParser.add<std::string>("kernel_file", 'k', "kernel recorded file name", true, "kernel_hard.txt");
    // cmdParser.parse_check(argc, argv);

    // model hyperparameters
    const std::string dummy_filename = "model_weights.bin";
    int num_layers = 24;
    int num_queries = 20;
    int hidden_dim = 2024;
    int ffn_dim = 6144;
    int seq_len = 64;
    int generated_tokens = 256;
    const int num_threads = 4;
    const int cpu_clk_idx = 12;
    const int ram_clk_idx = 11;
    const std::string kernel_file = "kernel_file_" + std::to_string(cpu_clk_idx) + "_" + std::to_string(ram_clk_idx) + ".txt";
    const std::string device_name = "Pixel9"; // fixed for testing
    // const std::string dummy_filename = cmdParser.get<std::string>("model");
    // int num_layers = cmdParser.get<int>("num_layers");
    // int num_queries = cmdParser.get<int>("num_queries");
    // int hidden_dim = cmdParser.get<int>("hidden_dim");
    // int ffn_dim = cmdParser.get<int>("ffn_size");
    // int seq_len = cmdParser.get<int>("input_tokens");
    // int generated_tokens = cmdParser.get<int>("output_tokens");
    // const int num_threads = cmdParser.get<int>("num_threads");
    // const int cpu_clk_idx = cmdParser.get<int>("cpu_clock");
    // const int ram_clk_idx = cmdParser.get<int>("ram_clock");
    // const std::string kernel_file = cmdParser.get<std::string>("kernel_file");
    // const std::string device_name = "Pixel9"; // fixed for testing

    // // DVFS setting
    // DVFS dvfs(device_name);
    // // cpu clock candidates
    // std::vector<int> freq_config = dvfs.get_cpu_freqs_conf(cpu_clk_idx);
    // for (auto f : freq_config) { std::cout << f << " "; }
    // std::cout << std::endl; // to validate (print freq-configuration)
    // // dvfs setting
    // dvfs.set_cpu_freq(freq_config);
    // dvfs.set_ram_freq(ram_clk_idx);

    // // start recording
    // std::thread record_thread = std::thread(record_hard_perf,
    //                                         std::ref(sigterm), kernel_file, device_name);

    // main procedure
    try {
        long long total_bytes_needed = (4LL * hidden_dim * hidden_dim + (1LL * hidden_dim * ffn_dim) + (1LL * ffn_dim * hidden_dim)) * sizeof(float);
        int model_size_mb = (total_bytes_needed / (1024 * 1024)) + 1;

        std::cout << "===== LLM Inference Pipeline Simulation (Multithreading) =====" << std::endl;
        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
        std::cout << "===== (CPU cores: " << num_threads << ") =====" << std::endl;

        std::cout << "Model dim: " << hidden_dim << ", FFN dim: " << ffn_dim << std::endl;
        std::cout << "# of layers: " << num_layers << " (operation simulation)" << std::endl;
        std::cout << "Required weights (per layer): " << total_bytes_needed / (1024 * 1024) << " MB" << std::endl;
        std::cout << "----------------------------------------------------" << std::endl;

        create_dummy_file(dummy_filename, model_size_mb);

        auto start_init = std::chrono::high_resolution_clock::now();

        std::ifstream model_file(dummy_filename, std::ios::binary);
        if (!model_file) throw std::runtime_error("Cannot open dummy model file.");

        Matrix W_q = initialize_matrix_from_file(hidden_dim, hidden_dim, model_file);
        Matrix W_k = initialize_matrix_from_file(hidden_dim, hidden_dim, model_file);
        Matrix W_v = initialize_matrix_from_file(hidden_dim, hidden_dim, model_file);
        Matrix W_o = initialize_matrix_from_file(hidden_dim, hidden_dim, model_file);

        Matrix W_ffn1 = initialize_matrix_from_file(hidden_dim, ffn_dim, model_file);
        Matrix W_ffn2 = initialize_matrix_from_file(ffn_dim, hidden_dim, model_file);
        model_file.close();

        auto end_init = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> init_duration = end_init - start_init;
        std::cout << "[Step 1&2: loading and initialization (opt.)]" << std::endl;
        std::cout << "Time to parse model and initialize: " << init_duration.count() << " ms" << std::endl << std::endl;

        // main inference simulation
        for (std::size_t i = 0; i < num_queries; ++i) {
            Matrix input_embeddings(seq_len, Vector(hidden_dim, 0.1f));
            auto start_prefill = std::chrono::high_resolution_clock::now();

            Matrix prefill_output = input_embeddings;
            for (int i = 0; i < num_layers; ++i) {
                prefill_output = transformer_layer_prefill(read(prefill_output), read(W_q), read(W_k), read(W_v), read(W_o), read(W_ffn1), read(W_ffn2), num_threads);
            }

            auto end_prefill = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> prefill_duration = end_prefill - start_prefill;

            std::cout << "\n[Step 3: Prefill (Compute-bound: GEMM)]" << std::endl;
            std::cout << "Total Time to " << seq_len << " tokens & " << num_layers << " layers: " << prefill_duration.count() << " ms" << std::endl
                      << std::endl;
            std::cout << "Throughput (pre): " << 1000 * seq_len / prefill_duration.count() << " tok/s" << std::endl;

            Vector current_token(hidden_dim, 0.1f);
            auto start_decode = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < generated_tokens; ++i) {
                Vector temp_token = current_token;
                for (int j = 0; j < num_layers; ++j) {
                    temp_token = transformer_layer_decode(read(temp_token), read(W_q), read(W_k), read(W_v), read(W_o), read(W_ffn1), read(W_ffn2), num_threads);
                }
                current_token = temp_token;
            }

            auto end_decode = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> decode_duration = end_decode - start_decode;

            std::cout << "[Step 4: Decode (Memory-bound: GEMV)]" << std::endl;
            std::cout << "Total Time to " << generated_tokens << " tokens & " << num_layers << " layers: " << decode_duration.count() << " ms" << std::endl;
            std::cout << "Time per output token: " << decode_duration.count() / generated_tokens << " ms" << std::endl;
            std::cout << "Throughput (dec): " << 1000 * generated_tokens / decode_duration.count() << " tok/s" << std::endl;
            std::cout << "----------------------------------------------------" << std::endl;
        }

        std::remove(dummy_filename.c_str());
        std::cout << "[Clean] Dummy model file '" << dummy_filename << "' is deleted." << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Critical error: " << e.what() << std::endl;
        return 1;
    }

    // simulation done
    sigterm = true;
    // dvfs.unset_cpu_freq();
    // dvfs.unset_ram_freq();
    // record_thread.join();

    std::cout << "DONE\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    return 0;
}