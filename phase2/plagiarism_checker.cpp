#include "plagiarism_checker.hpp"
// You should NOT add ANY other includes to this file.
// Do NOT add "using namespace std;".

// TODO: Implement the methods of the plagiarism_checker_t class

// Constructor for empty checker
plagiarism_checker_t::plagiarism_checker_t(void) : should_stop(false)
{
    worker_thread = std::thread(&plagiarism_checker_t::background_worker, this);
}

// Constructor with initial submissions
plagiarism_checker_t::plagiarism_checker_t(
    std::vector<std::shared_ptr<submission_t>> __submissions) : should_stop(false)
{
    if(__submissions.empty()) return;
    for (const auto &submission : __submissions)
    {
        submission_data data{
            submission,
            std::chrono::system_clock::now(),
            get_submission_tokens(submission->codefile)};
        base_submissions.push_back(data);
    }

    worker_thread = std::thread(&plagiarism_checker_t::background_worker, this);
}

// Destructor
plagiarism_checker_t::~plagiarism_checker_t(void)
{
    {
        std::lock_guard<std::mutex> lock(batch_mutex);
        should_stop = true;
    }

    cv.notify_one();

    worker_thread.join();
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        base_submissions.clear(); // Clear base submissions
    }
}

// Add new submission
void plagiarism_checker_t::add_submission(std::shared_ptr<submission_t> __submission)
{
    if(__submission==nullptr) return;
    {
        std::lock_guard<std::mutex> lock(batch_mutex);
        submission_data data{
            __submission,
            std::chrono::system_clock::now(),
            get_submission_tokens(__submission->codefile)
        };
        
        // adding to batch_submissions or rest_submissions
        if(batch_submissions.empty()) batch_submissions.push_back(data);
        else {
            if (std::chrono::duration_cast<std::chrono::seconds>(data.timestamp - batch_submissions[0].timestamp).count() <= 1) batch_submissions.push_back(data);
            else {
                rest_submissions.push_back(data);
                cv.notify_one();
            }
        }
    }
}

// BackGround Worker - Main processing loop
void plagiarism_checker_t::background_worker()
{
    while (true)
    {
        std::vector<submission_data> current_batch;

        // Get work from queue with proper synchronization
        {
            std::unique_lock<std::mutex> lock(batch_mutex);

            // Wait until there's work or should stop
            cv.wait(lock, [this]()
                    { return batch_submissions.size() > 0 || should_stop; });

            // Check termination condition
            if (should_stop && batch_submissions.size() == 0)
            {
                break;
            }
        }

        try
        {
            {   
                std::unique_lock<std::mutex> lock(batch_mutex);
                // copy batch to current_batch
                current_batch = batch_submissions;

                check_batch(current_batch);

                // removing the current submission from batch and adding the rest submissions if possible
                batch_submissions.erase(batch_submissions.begin());

                // case 1 batch is empty
                if (batch_submissions.size() == 0)
                {
                    // add rest submissions to batch submissions and pop from the rest submissions until rest submissions contain a window gap of one sec
                    if (!rest_submissions.empty())
                    {
                        batch_submissions.push_back(rest_submissions[0]);
                        rest_submissions.erase(rest_submissions.begin());
                    }
                    while (!rest_submissions.empty() &&
                           std::chrono::duration_cast<std::chrono::seconds>(rest_submissions[0].timestamp - batch_submissions[0].timestamp).count() <= 1)
                    {
                        batch_submissions.push_back(rest_submissions[0]);
                        rest_submissions.erase(rest_submissions.begin());
                    }
                }
                else
                {
                    while (!rest_submissions.empty() &&
                           std::chrono::duration_cast<std::chrono::seconds>(rest_submissions[0].timestamp - batch_submissions[0].timestamp).count() <= 1)
                    {
                        batch_submissions.push_back(rest_submissions[0]);
                        rest_submissions.erase(rest_submissions.begin());
                    }
                }
            }
            // Process the batch submission
            // check_batch(current_batch);
            // std::cout << "check_batch called" << std::endl;
        }
        catch (const std::exception &e)
        {
            // std::cerr << "Error processing submission: " << e.what() << std::endl;
        }
    }
}

// Process single batch
void plagiarism_checker_t::check_batch(std::vector<submission_data>& current_batch)
{ 
    submission_data current_submission =   current_batch[0];
    for(auto& curr: current_batch){
        if(sub_info.find(curr.submission) == sub_info.end()) {
            sub_info[curr.submission]={0,false};
        }
    }
    bool plag_in_batch =false;

    if(current_batch.size()>1){
         plag_in_batch = check_batch_submission(current_batch);
    }
    if(!sub_info[current_submission].second){
        submission_data& current_submission = current_batch[0];
        check_base_submission(current_submission);
    }

    //adding the current submission into base submissions
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        base_submissions.push_back(current_batch[0]);
    } 
}

bool plagiarism_checker_t::check_batch_submission(std::vector<submission_data>& current_batch)
{
    submission_data& current_submission = current_batch[0];
    int n = current_batch.size();
    for(int i=1;i<n;i++){
        submission_data& stored = current_batch[i];
        std::pair<bool,int> response = flag_if_plagiarized(current_submission, stored);
        current_submission.count += response.second;

        
        if(current_submission.count >= 20) {

            //flag 
            if(sub_info[current_submission.submission].second == false){
                if (current_submission.submission->student){
                    current_submission.submission->student->flag_student(stored.submission);
                }
                if (current_submission.submission->professor){
                    current_submission.submission->professor->flag_professor(stored.submission);
                }
            }
            sub_info[current_submission.submission].second=true;
            // return;
        }
    }
    return current_submission.is_flagged;
}


void plagiarism_checker_t::check_base_submission(submission_data& current_submission)
{
    {
        std::lock_guard<std::mutex> lock(data_mutex);

        // Check against all stored submissions
        if(current_submission.is_flagged == true) return;
        auto tc = current_submission.timestamp;

        int n = base_submissions.size();
        for(int i=0;i<n;i++){
            submission_data& stored = base_submissions[i];

            auto ts = stored.timestamp;
            auto t = std::chrono::duration_cast<std::chrono::seconds>(tc - ts).count();

            if(ts.time_since_epoch().count() == 0 || t > 1){
                std::pair<bool,int> response = flag_if_plagiarized(current_submission, stored);
                current_submission.count += response.second;

                if (sub_info[current_submission].second)
                {   
                    sub_info[current_submission.submission]={current_submission.count,true};
                    return;
                }
                if(current_submission.count >= 20) {

                    sub_info[current_submission.submission]={current_submission.count,true};
                    
                    //flag 
                    if(current_submission.is_flagged == false){
                        if (current_submission.submission->student){
                            current_submission.submission->student->flag_student(stored.submission);
                        }
                        if (current_submission.submission->professor){
                            current_submission.submission->professor->flag_professor(stored.submission);
                        }
                    }
                    
                    current_submission.is_flagged = true;
                    return;
                }
            }
        }
    }
}

// Flag submissions if plagiarized
std::pair<bool,int> plagiarism_checker_t::flag_if_plagiarized(
    const submission_data &current,
    const submission_data &reference)
{

    bool is_plagiarized = false;
    int count = 0;
    std::pair<int,int> p=find_exact_match(current.tokens, reference.tokens);

    count=p.first;
    int is_75_match=p.second;

    sub_info[current.submission].first+=count;
    sub_info[reference.submission].first+=count;

    // • There is an exact match of at least one pattern of length around 75 or more.
    if (is_75_match || count>=10)
    {
        is_plagiarized = true;
        sub_info[current.submission].second=true;
        sub_info[reference.submission].second=true;
    }

    

    if (is_plagiarized)
    {
        auto time_diff = current.timestamp - reference.timestamp;

        // Flag based on time difference
        if (std::chrono::duration_cast<std::chrono::seconds>(time_diff).count() >= 1)
        {
            // Flag only current submission
            if (current.submission->student)
            {
                current.submission->student->flag_student(current.submission);
            }
            if (current.submission->professor)
            {
                current.submission->professor->flag_professor(current.submission);
            }
        }
        else 
        {
            // Flag both submissions
            if (reference.submission->student)
            {
                reference.submission->student->flag_student(reference.submission);
            }
            if (reference.submission->professor)
            {
                reference.submission->professor->flag_professor(reference.submission);
            }
            
            if (current.submission->student)
            {
                current.submission->student->flag_student(current.submission);
            }
            if (current.submission->professor)
            {
                current.submission->professor->flag_professor(current.submission);
            }    
        }
    }
    
    return {is_plagiarized,count};
}

// find the count and is_75_match with find_exact_match function
std::pair<int, int> plagiarism_checker_t::find_exact_match(const std::vector<int> &tokens1,
                                     const std::vector<int> &tokens2)
{   
    Exact_Match Exact_Match_inst;
    std::pair<int,int> p=Exact_Match_inst.exact_match(tokens1,tokens2);
    return p;
}




// Token extraction helper
std::vector<int> plagiarism_checker_t::get_submission_tokens(const std::string &codefile)
{
    tokenizer_t tokenizer(codefile);
    return tokenizer.get_tokens();
}

// Implementation of Exact_Match class

// returns hash values of the text
// Hash computation is inspired from GeeksForGeeks - "https://www.geeksforgeeks.org/rabin-karp-algorithm-for-pattern-searching/"
std::map<int, std::vector<int>> Exact_Match::hash_text(const std::vector<int> text){
    std::map<int, std::vector<int>> hash_map;

    int n = text.size();
    int p = 5381, d = 33;
    int h = 1;

    for (int i = 0; i < k - 1; i++) h = (h * d) % p;

    int t = 0;
    for (int i = 0; i < k; i++) t = (d * t + text[i]) % p;

    if (hash_map.find(t) == hash_map.end()) hash_map[t] = {0};
    else hash_map[t].push_back(0);

    // Uses rolling hash function for hash computation efficiency
    for (int i = 1; i < n - k; i++){
        t = (d * (t - text[i - 1] * h) + text[i + k-1]) % p;
        if (t < 0) t = t + p;
        if (hash_map.find(t) == hash_map.end()) hash_map[t] = {i};
        else hash_map[t].push_back(i);
    }

    return hash_map;
}

// returns hash values of the pattern
// Hash computation is inspired from GeeksForGeeks - "https://www.geeksforgeeks.org/rabin-karp-algorithm-for-pattern-searching/"
std::vector<int> Exact_Match::hash_pattern(const std::vector<int> &pattern)
{
    int m = pattern.size();
    int h = 1;
    int p = 5381, d = 33;
    std::vector<int> hashes(m - k + 1);

    for (int i = 0; i < k - 1; i++) h = (h * d) % p;

    int t = 0;
    for (int i = 0; i < k; i++) t = (d * t + pattern[i]) % p;

    hashes[0] = t;
    // Uses rolling hash function for hash computation efficiency
    for (int i = 1; i < m - k; i++){
        t = (d * (t - pattern[i - 1] * h) + pattern[i + k-1]) % p;
        if (t < 0)
            t = t + p;
        hashes[i] = t;
    }

    return hashes;
}

// returns the length of the exact match
std::pair<int,int> Exact_Match::exact_match(const std::vector<int> &text, const std::vector<int> &pattern)
{   

    count=0;
    is_75_match=false;
    
    // calls the hash functions
    std::map<int, std::vector<int>> text_hash = hash_text(text);
    std::vector<int> pattern_hash = hash_pattern(pattern);
    int p = pattern.size();
    int t = text.size();

    std::vector<bool> visited(t, false);

    int hashp;
    int total_length = 0;

    // iterates over the pattern index by index if there is no match
    for (int i = 0; i < p - k; i++)
    {   
        // matches the hash value of pattern and text
        hashp = pattern_hash[i];
        if (text_hash.find(hashp) != text_hash.end())
        {
            std::vector<int> indices = text_hash[hashp];
            int max = k;
            int index = -1;

            // runs for maximum match length over each match of hash value
            for (auto j : indices)
            {
                int len = k;
                bool ismatch = true;

                // checks for feasible matches (considering overlap conditions)
                for (int z = 0; z < k; z++)
                {
                    if (text[j + z] != pattern[i + z]) ismatch = false;
                    if (visited[j + z] == true) ismatch = false;
                    if(ismatch == false) break;
                }
                
                // if match is feasible, checks for further indices 
                if(ismatch){
                    while ((j + len < t) && (text[j + len] == pattern[i + len]) && !(visited[j + len]))
                    {
                        len++;
                        if (len >= 75)
                            is_75_match=true;
                    }

                    if ((max <= len))
                    {
                        max = len;
                        index = j;
                    }
                }   
            }
            // Updates if the feasible match has maximum length and adds to the total_length
            if (index != -1)
            {
                for (int y = 0; y < max; y++) visited[index + y] = true;
                total_length = total_length + max;
                count+=max/k;
                i = i + max - 1;
            }
        }
    }
    //count = total_length / k;
    return {count,is_75_match};
}
// End TODO