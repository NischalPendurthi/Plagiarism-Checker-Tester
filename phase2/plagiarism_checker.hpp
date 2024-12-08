#include "structures.hpp"
// -----------------------------------------------------------------------------
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <queue>
#include <chrono>
#include <unordered_map>
#include <condition_variable>
#define k 15
// You are free to add any STL includes above this comment, below the --line--.
// DO NOT add "using namespace std;" or include any other files/libraries.
// Also DO NOT add the include "bits/stdc++.h"

// OPTIONAL: Add your helper functions and classes here


class Exact_Match{
    private:
        int count;
        bool is_75_match;
    public:
        std::pair<int,int> exact_match(const std::vector<int> &text, const std::vector<int> &pattern);
        std::vector<int> hash_pattern(const std::vector<int> &pattern);
        std::map<int, std::vector<int>> hash_text(const std::vector<int> text);
};

class plagiarism_checker_t {
    // You should NOT modify the public interface of this class.
public:
    plagiarism_checker_t(void);
    plagiarism_checker_t(std::vector<std::shared_ptr<submission_t>> 
                            __submissions);
    ~plagiarism_checker_t(void);
    void add_submission(std::shared_ptr<submission_t> __submission);

protected:
    // TODO: Add members and function signatures here
    
    // Structure to store submission with its timestamp and tokens
    struct submission_data {
        std::shared_ptr<submission_t> submission;
        std::chrono::system_clock::time_point timestamp;
        std::vector<int> tokens;
        int count=0;
        bool is_flagged=false;
    };

    // Threading and synchronization
    std::thread worker_thread;
    std::mutex batch_mutex;  //for batch submissions
    std::mutex data_mutex;  //for base submissions
    std::condition_variable cv;
    bool should_stop;

    // Data storage
    std::vector<submission_data> base_submissions;
    std::vector<submission_data> batch_submissions;
    std::vector<submission_data> rest_submissions;
    std::map<std::shared_ptr<submission_t>,std::pair<int,bool>> sub_info;
    
    //background checker
    void background_worker();

    // Thread management methods
    void check_batch(std::vector<submission_data>& current_batch);
    bool check_batch_submission(std::vector<submission_data>& current_batch);
    void check_base_submission(submission_data& current_submission);

    // Flagging helper
    std::pair<bool,int> flag_if_plagiarized(const submission_data& current,
                            const submission_data& reference);

    std::pair<int, int> find_exact_match(const std::vector<int> &tokens1,
                                     const std::vector<int> &tokens2);
    // Pattern matching utilities
    std::vector<int> get_submission_tokens(const std::string& code_file);
    // End TODO
};
