/**
 * ============================================
 *   M-PESA Terminal Client (C++)
 *   Communicates with Django REST backend
 * ============================================
 *
 * Compile:  g++ -o mpesa_client mpesa_client.cpp -lcurl -std=c++17
 * Run:      ./mpesa_client
 *
 * Dependencies: libcurl-dev
 *   Ubuntu: sudo apt-get install libcurl4-openssl-dev
 */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cstring>
#include <curl/curl.h>

// --- Config -----------------------------------------------------------------
const std::string BASE_URL  = "http://127.0.0.1:8000";
const std::string GREEN     = "\033[1;32m";
const std::string RED       = "\033[1;31m";
const std::string YELLOW    = "\033[1;33m";
const std::string CYAN      = "\033[1;36m";
const std::string BOLD      = "\033[1m";
const std::string RESET     = "\033[0m";
const std::string BLUE      = "\033[1;34m";

// --- Session state -----------------------------------------------------------
struct Session {
    std::string access_token;
    std::string refresh_token;
    std::string username;
    std::string full_name;
    std::string phone_number;
    bool logged_in = false;
};

Session g_session;

// --- HTTP helpers ------------------------------------------------------------
struct HttpResponse {
    long   status_code;
    std::string body;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}

HttpResponse http_post(const std::string& endpoint,
                       const std::string& json_body,
                       bool use_auth = false) {
    CURL* curl = curl_easy_init();
    HttpResponse resp{0, ""};
    if (!curl) return resp;

    std::string url = BASE_URL + endpoint;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (use_auth && !g_session.access_token.empty()) {
        std::string auth_header = "Authorization: Bearer " + g_session.access_token;
        headers = curl_slist_append(headers, auth_header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.body = "{\"error\":\"Connection failed: " + std::string(curl_easy_strerror(res)) + "\"}";
        resp.status_code = 0;
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

HttpResponse http_get(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    HttpResponse resp{0, ""};
    if (!curl) return resp;

    std::string url = BASE_URL + endpoint;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (!g_session.access_token.empty()) {
        std::string auth_header = "Authorization: Bearer " + g_session.access_token;
        headers = curl_slist_append(headers, auth_header.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.body = "{\"error\":\"Connection failed\"}";
        resp.status_code = 0;
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

// --- Minimal JSON parser helpers ---------------------------------------------
std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

std::string extract_json_value(const std::string& json, const std::string& key) {
    // For non-string values
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    // Skip whitespace
    while (pos < json.size() && json[pos] == ' ') pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        pos++;
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    }
    size_t end = json.find_first_of(",}", pos);
    if (end == std::string::npos) end = json.size();
    return json.substr(pos, end - pos);
}

// --- UI helpers --------------------------------------------------------------
void clear_screen() {
    std::cout << "\033[2J\033[1;1H";
}

void print_header() {
    std::cout << CYAN << "\n";
    std::cout << "  +------------------------------------------+\n";
    std::cout << "  ¦          ?? M-PESA TERMINAL v1.0          ¦\n";
    std::cout << "  +------------------------------------------+\n";
    std::cout << RESET;
}

void print_divider() {
    std::cout << CYAN << "  ------------------------------------------\n" << RESET;
}

void print_success(const std::string& msg) {
    std::cout << GREEN << "  ? " << msg << RESET << "\n";
}

void print_error(const std::string& msg) {
    std::cout << RED << "  ? " << msg << RESET << "\n";
}

void print_info(const std::string& msg) {
    std::cout << YELLOW << "  ? " << msg << RESET << "\n";
}

void press_enter() {
    std::cout << "\n" << CYAN << "  Press Enter to continue..." << RESET;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

std::string get_hidden_input(const std::string& prompt) {
    std::cout << YELLOW << "  " << prompt << RESET;
    std::string input;
    // Hide input (works on Linux/macOS)
    system("stty -echo");
    std::getline(std::cin, input);
    system("stty echo");
    std::cout << "\n";
    return input;
}

std::string get_input(const std::string& prompt) {
    std::cout << YELLOW << "  " << prompt << RESET;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

double get_amount(const std::string& prompt) {
    std::cout << YELLOW << "  " << prompt << RESET;
    std::string input;
    std::getline(std::cin, input);
    try {
        double val = std::stod(input);
        return val;
    } catch (...) {
        return -1.0;
    }
}

// --- Feature functions -------------------------------------------------------

void do_login() {
    clear_screen();
    print_header();
    std::cout << BOLD << "\n  ?? LOGIN TO M-PESA\n" << RESET;
    print_divider();

    std::string username = get_input("Username: ");
    std::string password = get_hidden_input("Password: ");

    if (username.empty() || password.empty()) {
        print_error("Username and password cannot be empty.");
        press_enter();
        return;
    }

    std::cout << "\n";
    print_info("Connecting to M-Pesa servers...");

    std::string body = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
    auto resp = http_post("/api/auth/login/", body);

    if (resp.status_code == 0) {
        print_error("Cannot connect to server. Make sure backend is running.");
        press_enter();
        return;
    }

    if (resp.status_code == 200) {
        g_session.access_token   = extract_json_value(resp.body, "access");
        g_session.refresh_token  = extract_json_value(resp.body, "refresh");
        g_session.username       = username;

        // Extract user fields
        g_session.full_name      = extract_json_value(resp.body, "full_name");
        g_session.phone_number   = extract_json_value(resp.body, "phone_number");
        g_session.logged_in      = true;

        std::string display_name = g_session.full_name.empty() ? g_session.username : g_session.full_name;
        print_success("Welcome back, " + display_name + "!");
        print_success("Phone: " + g_session.phone_number);
    } else {
        std::string err = extract_json_value(resp.body, "error");
        if (err.empty()) err = "Login failed (HTTP " + std::to_string(resp.status_code) + ")";
        print_error(err);
    }
    press_enter();
}

void do_logout() {
    std::string body = "{\"refresh\":\"" + g_session.refresh_token + "\"}";
    http_post("/api/auth/logout/", body, true);

    g_session = Session{};
    clear_screen();
    print_header();
    print_success("You have been logged out safely.");
    press_enter();
}

void do_check_balance() {
    clear_screen();
    print_header();
    std::cout << BOLD << "\n  ?? M-PESA BALANCE\n" << RESET;
    print_divider();

    auto resp = http_get("/api/balance/");

    if (resp.status_code == 0) {
        print_error("Cannot connect to server.");
        press_enter();
        return;
    }

    if (resp.status_code == 200) {
        std::string balance       = extract_json_value(resp.body, "balance");
        std::string phone         = extract_json_value(resp.body, "phone_number");
        std::string account_holder = extract_json_value(resp.body, "account_holder");

        std::cout << "\n";
        std::cout << CYAN  << "  Account Holder : " << BOLD << account_holder << RESET << "\n";
        std::cout << CYAN  << "  Phone Number   : " << BOLD << phone << RESET << "\n";
        std::cout << "\n";
        std::cout << GREEN << "  +------------------------------+\n";
        std::cout << GREEN << "  ¦  Available Balance           ¦\n";
        std::cout << GREEN << "  ¦  KES " << BOLD << std::left << std::setw(24) << balance << GREEN << "¦\n";
        std::cout << GREEN << "  +------------------------------+\n" << RESET;
    } else {
        std::string err = extract_json_value(resp.body, "error");
        print_error(err.empty() ? "Failed to fetch balance" : err);
    }
    press_enter();
}

void do_send_money() {
    clear_screen();
    print_header();
    std::cout << BOLD << "\n  ?? SEND MONEY\n" << RESET;
    print_divider();

    std::string recipient = get_input("Recipient Phone (e.g. 0722345678): ");
    if (recipient.empty()) { print_error("Recipient phone is required."); press_enter(); return; }

    double amount = get_amount("Amount (KES): ");
    if (amount <= 0) { print_error("Invalid amount."); press_enter(); return; }

    std::string desc = get_input("Description (optional): ");
    std::string pin  = get_hidden_input("Enter M-Pesa PIN: ");
    if (pin.empty()) { print_error("PIN is required."); press_enter(); return; }

    std::cout << "\n";
    print_info("Processing transaction...");

    std::ostringstream body;
    body << "{\"recipient_phone\":\"" << recipient
         << "\",\"amount\":" << std::fixed << std::setprecision(2) << amount
         << ",\"pin\":\"" << pin << "\""
         << ",\"description\":\"" << desc << "\"}";

    auto resp = http_post("/api/send/", body.str(), true);

    if (resp.status_code == 200) {
        std::string txn_id      = extract_json_value(resp.body, "transaction_id");
        std::string new_balance = extract_json_value(resp.body, "new_balance");
        print_success("Money sent successfully!");
        std::cout << CYAN << "\n  Transaction ID : " << BOLD << txn_id << RESET << "\n";
        std::cout << CYAN << "  Recipient      : " << BOLD << recipient << RESET << "\n";
        std::cout << CYAN << "  Amount         : " << BOLD << "KES " << amount << RESET << "\n";
        std::cout << CYAN << "  New Balance    : " << BOLD << "KES " << new_balance << RESET << "\n";
    } else {
        std::string err = extract_json_value(resp.body, "error");
        print_error(err.empty() ? "Transaction failed" : err);
    }
    press_enter();
}

void do_deposit() {
    clear_screen();
    print_header();
    std::cout << BOLD << "\n  ?? DEPOSIT MONEY\n" << RESET;
    print_divider();
    print_info("Simulate a deposit (e.g. from an M-Pesa agent)");
    std::cout << "\n";

    double amount = get_amount("Deposit Amount (KES): ");
    if (amount <= 0) { print_error("Invalid amount."); press_enter(); return; }

    std::string ref = get_input("Reference (optional): ");

    std::cout << "\n";
    print_info("Processing deposit...");

    std::ostringstream body;
    body << "{\"amount\":" << std::fixed << std::setprecision(2) << amount
         << ",\"reference\":\"" << ref << "\"}";

    auto resp = http_post("/api/deposit/", body.str(), true);

    if (resp.status_code == 200) {
        std::string txn_id      = extract_json_value(resp.body, "transaction_id");
        std::string new_balance = extract_json_value(resp.body, "new_balance");
        print_success("Deposit successful!");
        std::cout << CYAN << "\n  Transaction ID : " << BOLD << txn_id << RESET << "\n";
        std::cout << CYAN << "  Amount         : " << BOLD << "KES " << amount << RESET << "\n";
        std::cout << CYAN << "  New Balance    : " << BOLD << "KES " << new_balance << RESET << "\n";
    } else {
        std::string err = extract_json_value(resp.body, "error");
        print_error(err.empty() ? "Deposit failed" : err);
    }
    press_enter();
}

void do_withdraw() {
    clear_screen();
    print_header();
    std::cout << BOLD << "\n  ?? WITHDRAW CASH\n" << RESET;
    print_divider();

    double amount = get_amount("Withdrawal Amount (KES): ");
    if (amount <= 0) { print_error("Invalid amount."); press_enter(); return; }

    std::string pin = get_hidden_input("Enter M-Pesa PIN: ");
    if (pin.empty()) { print_error("PIN is required."); press_enter(); return; }

    std::cout << "\n";
    print_info("Processing withdrawal...");

    std::ostringstream body;
    body << "{\"amount\":" << std::fixed << std::setprecision(2) << amount
         << ",\"pin\":\"" << pin << "\""
         << ",\"description\":\"Cash withdrawal\"}";

    auto resp = http_post("/api/withdraw/", body.str(), true);

    if (resp.status_code == 200) {
        std::string txn_id      = extract_json_value(resp.body, "transaction_id");
        std::string new_balance = extract_json_value(resp.body, "new_balance");
        print_success("Withdrawal successful!");
        std::cout << CYAN << "\n  Transaction ID : " << BOLD << txn_id << RESET << "\n";
        std::cout << CYAN << "  Amount         : " << BOLD << "KES " << amount << RESET << "\n";
        std::cout << CYAN << "  New Balance    : " << BOLD << "KES " << new_balance << RESET << "\n";
    } else {
        std::string err = extract_json_value(resp.body, "error");
        print_error(err.empty() ? "Withdrawal failed" : err);
    }
    press_enter();
}

void do_transaction_history() {
    clear_screen();
    print_header();
    std::cout << BOLD << "\n  ?? TRANSACTION HISTORY\n" << RESET;
    print_divider();

    auto resp = http_get("/api/transactions/?limit=10");

    if (resp.status_code == 200) {
        // Simple extraction - parse transactions array
        size_t txn_start = resp.body.find("\"transactions\":");
        if (txn_start == std::string::npos) {
            print_info("No transactions found.");
            press_enter();
            return;
        }

        // Count and show raw info - simplified display
        std::string count_str = extract_json_value(resp.body, "count");
        std::cout << CYAN << "  Total transactions: " << BOLD << count_str << RESET << "\n\n";

        // Parse individual transactions
        std::string body_str = resp.body;
        size_t pos = 0;
        int shown = 0;

        while (shown < 10) {
            size_t type_pos = body_str.find("\"transaction_type\"", pos);
            if (type_pos == std::string::npos) break;

            // Extract fields
            std::string sub = body_str.substr(type_pos, 400);

            std::string t_type   = extract_json_value(sub, "transaction_type");
            std::string t_amount = extract_json_value(sub, "amount");
            std::string t_status = extract_json_value(sub, "status");
            std::string t_id     = extract_json_value(sub, "transaction_id");
            std::string t_date   = extract_json_value(sub, "created_at");
            std::string t_bal_af = extract_json_value(sub, "balance_after");

            // Color code by type
            std::string color = RESET;
            std::string icon  = "•";
            if (t_type == "SEND")     { color = RED;   icon = "?"; }
            if (t_type == "RECEIVE")  { color = GREEN; icon = "?"; }
            if (t_type == "DEPOSIT")  { color = GREEN; icon = "+"; }
            if (t_type == "WITHDRAW") { color = RED;   icon = "-"; }

            std::cout << color << "  " << icon << " " << BOLD << std::left << std::setw(8) << t_type << RESET;
            std::cout << CYAN  << " KES " << std::setw(12) << t_amount;
            std::cout << RESET << " Bal: " << std::setw(10) << t_bal_af;
            std::cout << YELLOW << " " << t_id << RESET << "\n";

            pos = type_pos + 1;
            shown++;
        }

        if (shown == 0) {
            print_info("No transactions yet.");
        }
    } else {
        print_error("Failed to fetch transaction history.");
    }
    press_enter();
}

// --- Menus -------------------------------------------------------------------

void show_main_menu() {
    while (true) {
        clear_screen();
        print_header();

        std::cout << CYAN << "\n  Logged in as: " << BOLD << g_session.full_name;
        if (g_session.full_name.empty()) std::cout << g_session.username;
        std::cout << RESET << CYAN << "  [" << g_session.phone_number << "]\n\n" << RESET;

        std::cout << BOLD << "  MAIN MENU\n" << RESET;
        print_divider();
        std::cout << "  [1] ?? Check Balance\n";
        std::cout << "  [2] ?? Send Money\n";
        std::cout << "  [3] ?? Deposit\n";
        std::cout << "  [4] ?? Withdraw\n";
        std::cout << "  [5] ?? Transaction History\n";
        std::cout << "  [6] ?? Logout\n";
        print_divider();

        std::string choice = get_input("Select option (1-6): ");

        if      (choice == "1") do_check_balance();
        else if (choice == "2") do_send_money();
        else if (choice == "3") do_deposit();
        else if (choice == "4") do_withdraw();
        else if (choice == "5") do_transaction_history();
        else if (choice == "6") { do_logout(); break; }
        else { print_error("Invalid option. Try again."); press_enter(); }
    }
}

void show_welcome_menu() {
    while (true) {
        clear_screen();
        print_header();

        std::cout << "\n  " << CYAN << "Welcome to M-Pesa Terminal" << RESET << "\n";
        std::cout << "  The fastest way to manage your money.\n\n";
        print_divider();
        std::cout << "  [1] ?? Login\n";
        std::cout << "  [0] ? Exit\n";
        print_divider();

        std::string choice = get_input("Select option: ");

        if (choice == "1") {
            do_login();
            if (g_session.logged_in) {
                show_main_menu();
            }
        } else if (choice == "0") {
            clear_screen();
            print_header();
            std::cout << "\n  " << GREEN << "Thank you for using M-Pesa Terminal!" << RESET << "\n\n";
            break;
        } else {
            print_error("Invalid option.");
            press_enter();
        }
    }
}

// --- Entry point -------------------------------------------------------------
int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    show_welcome_menu();
    curl_global_cleanup();
    return 0;
}
