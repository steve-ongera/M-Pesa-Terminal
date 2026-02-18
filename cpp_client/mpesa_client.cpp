/**
 * ============================================
 *   M-PESA Terminal Client
 *   Built for Dev-C++ (Windows)
 * ============================================
 *
 * HOW TO SET UP IN DEV-C++:
 * --------------------------------------------------
 * 1. Download libcurl for Windows (MinGW):
 *    https://curl.se/windows/
 *    Extract to e.g. C:\curl\
 *
 * 2. In Dev-C++ -> Tools -> Compiler Options:
 *    Add to "Linker" tab:
 *       -LC:\curl\lib -lcurl -lws2_32
 *    Add to "Directories -> C++ Includes":
 *       C:\curl\include
 *
 * 3. Copy curl\bin\libcurl.dll next to your .exe
 *
 * --------------------------------------------------
 * BACKEND: Run Django server on localhost:8000
 *   python manage.py runserver 0.0.0.0:8000
 * --------------------------------------------------
 */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <windows.h>
#include <conio.h>
#include <curl/curl.h>

using namespace std;

// --- Server config ------------------------------------------------------------
const string BASE_URL = "http://127.0.0.1:8000";

// --- Windows Console Colors --------------------------------------------------
HANDLE hConsole;

#define CLR_DEFAULT  7
#define CLR_WHITE   15
#define CLR_GREEN   10
#define CLR_RED     12
#define CLR_YELLOW  14
#define CLR_CYAN    11

void set_color(int color) {
    SetConsoleTextAttribute(hConsole, color);
}

// --- Session -----------------------------------------------------------------
struct Session {
    string access_token;
    string refresh_token;
    string username;
    string full_name;
    string phone_number;
    bool   logged_in = false;
};

Session g_session;

// --- HTTP ---------------------------------------------------------------------
struct HttpResponse {
    long   status_code;
    string body;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* s) {
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}

HttpResponse http_post(const string& endpoint,
                       const string& json_body,
                       bool use_auth = false) {
    CURL* curl = curl_easy_init();
    HttpResponse resp{0, ""};
    if (!curl) { resp.body = "{\"error\":\"curl init failed\"}"; return resp; }

    string url = BASE_URL + endpoint;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (use_auth && !g_session.access_token.empty()) {
        string h = "Authorization: Bearer " + g_session.access_token;
        headers = curl_slist_append(headers, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.body = "{\"error\":\"" + string(curl_easy_strerror(res)) + "\"}";
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

HttpResponse http_get(const string& endpoint) {
    CURL* curl = curl_easy_init();
    HttpResponse resp{0, ""};
    if (!curl) { resp.body = "{\"error\":\"curl init failed\"}"; return resp; }

    string url = BASE_URL + endpoint;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (!g_session.access_token.empty()) {
        string h = "Authorization: Bearer " + g_session.access_token;
        headers = curl_slist_append(headers, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_HTTPGET,        1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.body = "{\"error\":\"Connection failed - is the Django server running?\"}";
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status_code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

// --- JSON helper -------------------------------------------------------------
string json_get(const string& json, const string& key) {
    string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == string::npos) return "";
    pos += search.length();
    while (pos < json.size() && json[pos] == ' ') pos++;
    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        pos++;
        string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) pos++;
            result += json[pos++];
        }
        return result;
    }
    size_t end = json.find_first_of(",}\n]", pos);
    if (end == string::npos) end = json.size();
    string val = json.substr(pos, end - pos);
    while (!val.empty() && (val.back() == ' ' || val.back() == '\r')) val.pop_back();
    return val;
}

// --- UI helpers --------------------------------------------------------------
void clear_screen() { system("cls"); }

void print_header() {
    set_color(CLR_CYAN);
    cout << "\n";
    cout << "  +==========================================+\n";
    cout << "  |         M-PESA TERMINAL  v1.0            |\n";
    cout << "  |           Powered by Django API          |\n";
    cout << "  +==========================================+\n";
    set_color(CLR_DEFAULT);
}

void print_divider() {
    set_color(CLR_CYAN);
    cout << "  ------------------------------------------\n";
    set_color(CLR_DEFAULT);
}

void print_success(const string& msg) {
    set_color(CLR_GREEN);  cout << "  [OK]    " << msg << "\n"; set_color(CLR_DEFAULT);
}
void print_error(const string& msg) {
    set_color(CLR_RED);    cout << "  [ERROR] " << msg << "\n"; set_color(CLR_DEFAULT);
}
void print_info(const string& msg) {
    set_color(CLR_YELLOW); cout << "  [INFO]  " << msg << "\n"; set_color(CLR_DEFAULT);
}

void press_enter() {
    set_color(CLR_CYAN);
    cout << "\n  Press Enter to continue...";
    set_color(CLR_DEFAULT);
    if (cin.peek() == '\n') cin.ignore();
    cin.get();
}

string get_input(const string& prompt) {
    set_color(CLR_YELLOW); cout << "  " << prompt;
    set_color(CLR_WHITE);
    string input;
    getline(cin, input);
    set_color(CLR_DEFAULT);
    return input;
}

// Hidden input using _getch() — works in Dev-C++ on Windows
string get_hidden_input(const string& prompt) {
    set_color(CLR_YELLOW); cout << "  " << prompt;
    set_color(CLR_WHITE);
    string input;
    char ch;
    while ((ch = (char)_getch()) != '\r') {
        if (ch == '\b') {
            if (!input.empty()) {
                input.pop_back();
                cout << "\b \b";
            }
        } else if (ch >= 32 && ch < 127) {
            input += ch;
            cout << '*';
        }
    }
    cout << "\n";
    set_color(CLR_DEFAULT);
    return input;
}

double get_amount(const string& prompt) {
    set_color(CLR_YELLOW); cout << "  " << prompt;
    set_color(CLR_WHITE);
    string input;
    getline(cin, input);
    set_color(CLR_DEFAULT);
    if (input.empty()) return -1.0;
    try { return stod(input); } catch (...) { return -1.0; }
}

// --- Feature: Login -----------------------------------------------------------
void do_login() {
    clear_screen();
    print_header();
    set_color(CLR_WHITE); cout << "\n  === LOGIN ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    string username = get_input("Username: ");
    string password = get_hidden_input("Password: ");

    if (username.empty() || password.empty()) {
        print_error("Username and password cannot be empty.");
        press_enter(); return;
    }

    cout << "\n";
    print_info("Connecting to M-Pesa server...");

    string body = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
    HttpResponse resp = http_post("/api/auth/login/", body);

    if (resp.status_code == 0) {
        print_error("Cannot reach server. Make sure Django is running on port 8000.");
        press_enter(); return;
    }

    if (resp.status_code == 200) {
        g_session.access_token  = json_get(resp.body, "access");
        g_session.refresh_token = json_get(resp.body, "refresh");
        g_session.username      = username;
        g_session.full_name     = json_get(resp.body, "full_name");
        g_session.phone_number  = json_get(resp.body, "phone_number");
        g_session.logged_in     = true;

        string display = g_session.full_name.empty() ? g_session.username : g_session.full_name;
        print_success("Welcome back, " + display + "!");
        print_success("Phone: " + g_session.phone_number);
    } else {
        string err = json_get(resp.body, "error");
        if (err.empty()) err = "Login failed (HTTP " + to_string(resp.status_code) + ")";
        print_error(err);
    }
    press_enter();
}

// --- Feature: Logout ----------------------------------------------------------
void do_logout() {
    string body = "{\"refresh\":\"" + g_session.refresh_token + "\"}";
    http_post("/api/auth/logout/", body, true);
    g_session = Session{};

    clear_screen();
    print_header();
    print_success("You have been logged out safely. Goodbye!");
    press_enter();
}

// --- Feature: Balance ---------------------------------------------------------
void do_check_balance() {
    clear_screen();
    print_header();
    set_color(CLR_WHITE); cout << "\n  === M-PESA BALANCE ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    HttpResponse resp = http_get("/api/balance/");

    if (resp.status_code == 0) {
        print_error("Cannot connect to server.");
        press_enter(); return;
    }

    if (resp.status_code == 200) {
        string balance = json_get(resp.body, "balance");
        string phone   = json_get(resp.body, "phone_number");
        string holder  = json_get(resp.body, "account_holder");

        cout << "\n";
        set_color(CLR_CYAN);  cout << "  Account Holder : "; set_color(CLR_WHITE); cout << holder << "\n";
        set_color(CLR_CYAN);  cout << "  Phone Number   : "; set_color(CLR_WHITE); cout << phone  << "\n\n";
        set_color(CLR_GREEN);
        cout << "  +================================+\n";
        cout << "  |  Available Balance             |\n";
        cout << "  |  KES "; set_color(CLR_WHITE);
        cout << left << setw(26) << balance;
        set_color(CLR_GREEN);
        cout << "|\n  +================================+\n";
        set_color(CLR_DEFAULT);
    } else {
        string err = json_get(resp.body, "error");
        print_error(err.empty() ? "Failed to fetch balance." : err);
    }
    press_enter();
}

// --- Feature: Send Money ------------------------------------------------------
void do_send_money() {
    clear_screen();
    print_header();
    set_color(CLR_WHITE); cout << "\n  === SEND MONEY ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    string recipient = get_input("Recipient Phone (e.g. 0722345678): ");
    if (recipient.empty()) { print_error("Recipient phone is required."); press_enter(); return; }

    double amount = get_amount("Amount (KES): ");
    if (amount <= 0) { print_error("Invalid amount entered."); press_enter(); return; }

    string desc = get_input("Description (optional, Enter to skip): ");
    string pin  = get_hidden_input("Enter your M-Pesa PIN: ");
    if (pin.empty()) { print_error("PIN is required."); press_enter(); return; }

    cout << "\n"; print_info("Processing your transaction...");

    ostringstream body;
    body << "{\"recipient_phone\":\"" << recipient
         << "\",\"amount\":"          << fixed << setprecision(2) << amount
         << ",\"pin\":\""             << pin << "\""
         << ",\"description\":\""     << desc << "\"}";

    HttpResponse resp = http_post("/api/send/", body.str(), true);

    if (resp.status_code == 200) {
        string txn_id  = json_get(resp.body, "transaction_id");
        string new_bal = json_get(resp.body, "new_balance");
        print_success("Money sent successfully!");
        cout << "\n";
        set_color(CLR_CYAN);  cout << "  Transaction ID : "; set_color(CLR_WHITE); cout << txn_id << "\n";
        set_color(CLR_CYAN);  cout << "  Sent To        : "; set_color(CLR_WHITE); cout << recipient << "\n";
        set_color(CLR_CYAN);  cout << "  Amount Sent    : "; set_color(CLR_WHITE); cout << "KES " << fixed << setprecision(2) << amount << "\n";
        set_color(CLR_CYAN);  cout << "  New Balance    : "; set_color(CLR_GREEN); cout << "KES " << new_bal << "\n";
        set_color(CLR_DEFAULT);
    } else {
        string err = json_get(resp.body, "error");
        print_error(err.empty() ? "Transaction failed." : err);
    }
    press_enter();
}

// --- Feature: Deposit ---------------------------------------------------------
void do_deposit() {
    clear_screen();
    print_header();
    set_color(CLR_WHITE); cout << "\n  === DEPOSIT MONEY ===\n\n"; set_color(CLR_DEFAULT);
    print_info("Simulate a cash deposit (e.g. via M-Pesa agent)");
    print_divider();

    double amount = get_amount("Deposit Amount (KES): ");
    if (amount <= 0) { print_error("Invalid amount."); press_enter(); return; }

    string ref = get_input("Reference/Agent Code (optional): ");

    cout << "\n"; print_info("Processing deposit...");

    ostringstream body;
    body << "{\"amount\":"       << fixed << setprecision(2) << amount
         << ",\"reference\":\"" << ref << "\"}";

    HttpResponse resp = http_post("/api/deposit/", body.str(), true);

    if (resp.status_code == 200) {
        string txn_id  = json_get(resp.body, "transaction_id");
        string new_bal = json_get(resp.body, "new_balance");
        print_success("Deposit successful!");
        cout << "\n";
        set_color(CLR_CYAN);  cout << "  Transaction ID : "; set_color(CLR_WHITE); cout << txn_id << "\n";
        set_color(CLR_CYAN);  cout << "  Amount         : "; set_color(CLR_WHITE); cout << "KES " << fixed << setprecision(2) << amount << "\n";
        set_color(CLR_CYAN);  cout << "  New Balance    : "; set_color(CLR_GREEN); cout << "KES " << new_bal << "\n";
        set_color(CLR_DEFAULT);
    } else {
        string err = json_get(resp.body, "error");
        print_error(err.empty() ? "Deposit failed." : err);
    }
    press_enter();
}

// --- Feature: Withdraw --------------------------------------------------------
void do_withdraw() {
    clear_screen();
    print_header();
    set_color(CLR_WHITE); cout << "\n  === WITHDRAW CASH ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    double amount = get_amount("Withdrawal Amount (KES): ");
    if (amount <= 0) { print_error("Invalid amount."); press_enter(); return; }

    string pin = get_hidden_input("Enter your M-Pesa PIN: ");
    if (pin.empty()) { print_error("PIN is required."); press_enter(); return; }

    cout << "\n"; print_info("Processing withdrawal...");

    ostringstream body;
    body << "{\"amount\":"     << fixed << setprecision(2) << amount
         << ",\"pin\":\""      << pin << "\""
         << ",\"description\":\"Cash withdrawal\"}";

    HttpResponse resp = http_post("/api/withdraw/", body.str(), true);

    if (resp.status_code == 200) {
        string txn_id  = json_get(resp.body, "transaction_id");
        string new_bal = json_get(resp.body, "new_balance");
        print_success("Withdrawal successful!");
        cout << "\n";
        set_color(CLR_CYAN);  cout << "  Transaction ID : "; set_color(CLR_WHITE); cout << txn_id << "\n";
        set_color(CLR_CYAN);  cout << "  Amount         : "; set_color(CLR_WHITE); cout << "KES " << fixed << setprecision(2) << amount << "\n";
        set_color(CLR_CYAN);  cout << "  New Balance    : "; set_color(CLR_GREEN); cout << "KES " << new_bal << "\n";
        set_color(CLR_DEFAULT);
    } else {
        string err = json_get(resp.body, "error");
        print_error(err.empty() ? "Withdrawal failed." : err);
    }
    press_enter();
}

// --- Feature: Transaction History --------------------------------------------
void do_transaction_history() {
    clear_screen();
    print_header();
    set_color(CLR_WHITE); cout << "\n  === TRANSACTION HISTORY (Last 10) ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    HttpResponse resp = http_get("/api/transactions/?limit=10");

    if (resp.status_code != 200) {
        print_error("Failed to fetch transactions.");
        press_enter(); return;
    }

    string count_str = json_get(resp.body, "count");
    set_color(CLR_CYAN); cout << "  Total recorded: "; set_color(CLR_WHITE); cout << count_str << "\n\n";

    // Table header
    set_color(CLR_YELLOW);
    cout << "  " << left
         << setw(10) << "TYPE"
         << setw(14) << "AMOUNT(KES)"
         << setw(14) << "BAL AFTER"
         << "TRANSACTION ID\n";
    set_color(CLR_CYAN);
    cout << "  " << string(58, '-') << "\n";
    set_color(CLR_DEFAULT);

    string body_str = resp.body;
    size_t pos = 0;
    int shown = 0;

    while (shown < 10) {
        size_t type_pos = body_str.find("\"transaction_type\"", pos);
        if (type_pos == string::npos) break;

        string sub     = body_str.substr(type_pos, 500);
        string t_type  = json_get(sub, "transaction_type");
        string t_amt   = json_get(sub, "amount");
        string t_bal   = json_get(sub, "balance_after");
        string t_id    = json_get(sub, "transaction_id");

        if (t_type == "SEND" || t_type == "WITHDRAW")
            set_color(CLR_RED);
        else
            set_color(CLR_GREEN);

        cout << "  " << left
             << setw(10) << t_type
             << setw(14) << t_amt
             << setw(14) << t_bal
             << t_id << "\n";

        set_color(CLR_DEFAULT);
        pos = type_pos + 1;
        shown++;
    }

    if (shown == 0) print_info("No transactions on record yet.");
    press_enter();
}

// --- Main Menu (after login) --------------------------------------------------
void show_main_menu() {
    while (true) {
        clear_screen();
        print_header();

        set_color(CLR_CYAN); cout << "\n  Logged in: ";
        set_color(CLR_WHITE);
        string display = g_session.full_name.empty() ? g_session.username : g_session.full_name;
        cout << display << "  [" << g_session.phone_number << "]\n\n";
        set_color(CLR_DEFAULT);

        set_color(CLR_WHITE); cout << "  MAIN MENU\n"; set_color(CLR_DEFAULT);
        print_divider();
        cout << "  [1]  Check Balance\n";
        cout << "  [2]  Send Money\n";
        cout << "  [3]  Deposit\n";
        cout << "  [4]  Withdraw\n";
        cout << "  [5]  Transaction History\n";
        cout << "  [6]  Logout\n";
        print_divider();

        string choice = get_input("Select option (1-6): ");

        if      (choice == "1") do_check_balance();
        else if (choice == "2") do_send_money();
        else if (choice == "3") do_deposit();
        else if (choice == "4") do_withdraw();
        else if (choice == "5") do_transaction_history();
        else if (choice == "6") { do_logout(); break; }
        else {
            print_error("Invalid option. Please choose 1 to 6.");
            press_enter();
        }
    }
}

// --- Welcome Screen -----------------------------------------------------------
void show_welcome_menu() {
    while (true) {
        clear_screen();
        print_header();

        set_color(CLR_CYAN);  cout << "\n  Welcome to M-Pesa Terminal\n";
        set_color(CLR_DEFAULT); cout << "  Manage your M-Pesa account from your terminal.\n\n";
        print_divider();
        cout << "  [1]  Login\n";
        cout << "  [0]  Exit\n";
        print_divider();

        string choice = get_input("Select option: ");

        if (choice == "1") {
            do_login();
            if (g_session.logged_in) show_main_menu();
        } else if (choice == "0") {
            clear_screen();
            print_header();
            set_color(CLR_GREEN); cout << "\n  Thank you for using M-Pesa Terminal!\n\n";
            set_color(CLR_DEFAULT);
            break;
        } else {
            print_error("Invalid option. Press 1 to login or 0 to exit.");
            press_enter();
        }
    }
}

// --- Entry Point --------------------------------------------------------------
int main() {
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(CP_UTF8);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    show_welcome_menu();

    curl_global_cleanup();
    return 0;
}
