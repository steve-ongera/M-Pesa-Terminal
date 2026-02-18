/**
 * ============================================
 *   M-PESA Terminal Client
 *   For Dev-C++ with TDM-GCC 4.9.2 (Windows)
 *   Uses WinHTTP - NO libcurl needed
 * ============================================
 *
 *  SETUP IN DEV-C++ (do this once):
 *  ---------------------------------------------
 *  1. Tools -> Compiler Options -> General tab
 *     In "Add the following commands..." box add:
 *         -std=c++11
 *
 *  2. Project -> Project Options -> Parameters tab
 *     In the "Linker" box add:
 *         -lwinhttp -lws2_32
 *
 *  3. Press F9 to Build & Run
 *  ---------------------------------------------
 *  Make sure Django backend is running:
 *    python manage.py runserver 0.0.0.0:8000
 * ============================================
 */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <conio.h>

using namespace std;

// --- Server config ------------------------------------------------------------
const wstring      SERVER_HOST = L"127.0.0.1";
const INTERNET_PORT SERVER_PORT = 8000;

// --- Console colors -----------------------------------------------------------
HANDLE hConsole;
#define CLR_DEFAULT  7
#define CLR_WHITE   15
#define CLR_GREEN   10
#define CLR_RED     12
#define CLR_YELLOW  14
#define CLR_CYAN    11

void set_color(int c) { SetConsoleTextAttribute(hConsole, c); }

// --- Session ------------------------------------------------------------------
struct Session {
    string access_token;
    string refresh_token;
    string username;
    string full_name;
    string phone_number;
    bool   logged_in;

    Session() : logged_in(false) {}   // C++98 constructor instead of = false
};
Session g_session;

// --- HTTP response ------------------------------------------------------------
struct HttpResponse {
    int    status_code;
    string body;

    HttpResponse() : status_code(0) {}
};

// --- String helpers (C++98 safe) ---------------------------------------------

// Replace C++11 to_string for integers
string int_to_str(int n) {
    ostringstream ss;
    ss << n;
    return ss.str();
}

// Replace C++11 stod
double str_to_double(const string& s) {
    double val = 0.0;
    istringstream ss(s);
    ss >> val;
    return (ss.fail()) ? -1.0 : val;
}

// Replace C++11 string::back() and pop_back()
void rtrim(string& s) {
    while (!s.empty() && (s[s.size()-1] == ' ' || s[s.size()-1] == '\r' || s[s.size()-1] == '\n'))
        s.erase(s.size()-1, 1);
}

// Replace C++11 nullptr with NULL for WinAPI calls
wstring to_wide(const string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    wstring result(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &result[0], len);
    return result;
}

// --- WinHTTP request ---------------------------------------------------------
HttpResponse winhttp_request(const string& method,
                              const string& path,
                              const string& body,
                              const string& auth_token) {
    HttpResponse resp;

    HINTERNET hSession = WinHttpOpen(
        L"MPesaClient/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
        resp.body = "{\"error\":\"WinHTTP init failed\"}";
        return resp;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, SERVER_HOST.c_str(), SERVER_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        resp.body = "{\"error\":\"Cannot connect - is Django running on port 8000?\"}";
        return resp;
    }

    wstring wpath   = to_wide(path);
    wstring wmethod = to_wide(method);

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        wmethod.c_str(),
        wpath.c_str(),
        NULL,                         // HTTP version (default)
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0);                           // 0 = plain HTTP, not HTTPS

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.body = "{\"error\":\"Failed to create request\"}";
        return resp;
    }

    // Build request headers
    wstring headers = L"Content-Type: application/json\r\n";
    if (!auth_token.empty()) {
        headers += L"Authorization: Bearer " + to_wide(auth_token) + L"\r\n";
    }

    BOOL sent = FALSE;
    if (!body.empty()) {
        sent = WinHttpSendRequest(
            hRequest,
            headers.c_str(), (DWORD)headers.size(),
            (LPVOID)body.c_str(), (DWORD)body.size(),
            (DWORD)body.size(), 0);
    } else {
        sent = WinHttpSendRequest(
            hRequest,
            headers.c_str(), (DWORD)headers.size(),
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }

    if (!sent || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.body = "{\"error\":\"Request failed - make sure server is running\"}";
        return resp;
    }

    // Read HTTP status code
    DWORD status_code = 0;
    DWORD status_size = sizeof(DWORD);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX);
    resp.status_code = (int)status_code;

    // Read response body
    DWORD bytes_available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
        vector<char> buffer(bytes_available + 1, '\0');
        DWORD bytes_read = 0;
        WinHttpReadData(hRequest, &buffer[0], bytes_available, &bytes_read);
        resp.body.append(&buffer[0], bytes_read);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}

// Convenience wrappers
HttpResponse http_post(const string& path, const string& body, bool auth = false) {
    return winhttp_request("POST", path, body, auth ? g_session.access_token : "");
}
HttpResponse http_get(const string& path) {
    return winhttp_request("GET", path, "", g_session.access_token);
}

// --- JSON helper -------------------------------------------------------------
string json_get(const string& json, const string& key) {
    string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == string::npos) return "";
    pos += search.size();
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
    rtrim(val);
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
    string s;
    getline(cin, s);
    set_color(CLR_DEFAULT);
    return s;
}

// Hidden input using _getch(), shows * instead of characters
string get_hidden(const string& prompt) {
    set_color(CLR_YELLOW); cout << "  " << prompt;
    set_color(CLR_WHITE);
    string s;
    char ch;
    while ((ch = (char)_getch()) != '\r') {
        if (ch == '\b') {
            if (!s.empty()) {
                s.erase(s.size()-1, 1);   // C++98 safe erase
                cout << "\b \b";
            }
        } else if (ch >= 32 && ch < 127) {
            s += ch;
            cout << '*';
        }
    }
    cout << "\n";
    set_color(CLR_DEFAULT);
    return s;
}

double get_amount(const string& prompt) {
    string s = get_input(prompt);
    if (s.empty()) return -1.0;
    return str_to_double(s);
}

// --- Feature: Login -----------------------------------------------------------
void do_login() {
    clear_screen(); print_header();
    set_color(CLR_WHITE); cout << "\n  === LOGIN ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    string username = get_input("Username: ");
    string password = get_hidden("Password: ");
    if (username.empty() || password.empty()) {
        print_error("Username and password cannot be empty.");
        press_enter(); return;
    }

    cout << "\n"; print_info("Connecting to M-Pesa server...");

    string body = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
    HttpResponse r = http_post("/api/auth/login/", body);

    if (r.status_code == 0) {
        print_error("Cannot reach server. Run: python manage.py runserver");
        press_enter(); return;
    }

    if (r.status_code == 200) {
        g_session.access_token  = json_get(r.body, "access");
        g_session.refresh_token = json_get(r.body, "refresh");
        g_session.username      = username;
        g_session.full_name     = json_get(r.body, "full_name");
        g_session.phone_number  = json_get(r.body, "phone_number");
        g_session.logged_in     = true;
        string name = g_session.full_name.empty() ? username : g_session.full_name;
        print_success("Welcome, " + name + "!");
        print_success("Phone: " + g_session.phone_number);
    } else {
        string err = json_get(r.body, "error");
        if (err.empty()) err = "Login failed (HTTP " + int_to_str(r.status_code) + ")";
        print_error(err);
    }
    press_enter();
}

// --- Feature: Logout ----------------------------------------------------------
void do_logout() {
    http_post("/api/auth/logout/", "{\"refresh\":\"" + g_session.refresh_token + "\"}", true);
    g_session = Session();   // C++98: Session() not Session{}
    clear_screen(); print_header();
    print_success("Logged out successfully. Goodbye!");
    press_enter();
}

// --- Feature: Balance ---------------------------------------------------------
void do_balance() {
    clear_screen(); print_header();
    set_color(CLR_WHITE); cout << "\n  === M-PESA BALANCE ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    HttpResponse r = http_get("/api/balance/");
    if (r.status_code == 200) {
        string balance = json_get(r.body, "balance");
        string phone   = json_get(r.body, "phone_number");
        string holder  = json_get(r.body, "account_holder");
        cout << "\n";
        set_color(CLR_CYAN);  cout << "  Account Holder : "; set_color(CLR_WHITE); cout << holder << "\n";
        set_color(CLR_CYAN);  cout << "  Phone Number   : "; set_color(CLR_WHITE); cout << phone  << "\n\n";
        set_color(CLR_GREEN);
        cout << "  +================================+\n";
        cout << "  |  Available Balance             |\n";
        cout << "  |  KES ";
        set_color(CLR_WHITE); cout << left << setw(26) << balance;
        set_color(CLR_GREEN); cout << "|\n";
        cout << "  +================================+\n";
        set_color(CLR_DEFAULT);
    } else {
        string err = json_get(r.body, "error");
        print_error(err.empty() ? "Failed to get balance." : err);
    }
    press_enter();
}

// --- Feature: Send Money ------------------------------------------------------
void do_send() {
    clear_screen(); print_header();
    set_color(CLR_WHITE); cout << "\n  === SEND MONEY ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    string recipient = get_input("Recipient Phone (e.g. 0722345678): ");
    if (recipient.empty()) { print_error("Phone is required."); press_enter(); return; }

    double amount = get_amount("Amount (KES): ");
    if (amount <= 0) { print_error("Invalid amount."); press_enter(); return; }

    string desc = get_input("Description (optional, Enter to skip): ");
    string pin  = get_hidden("Enter M-Pesa PIN: ");
    if (pin.empty()) { print_error("PIN is required."); press_enter(); return; }

    cout << "\n"; print_info("Processing transaction...");

    ostringstream body;
    body << "{\"recipient_phone\":\"" << recipient
         << "\",\"amount\":"          << fixed << setprecision(2) << amount
         << ",\"pin\":\""             << pin   << "\""
         << ",\"description\":\""     << desc  << "\"}";

    HttpResponse r = http_post("/api/send/", body.str(), true);
    if (r.status_code == 200) {
        print_success("Money sent successfully!");
        cout << "\n";
        set_color(CLR_CYAN); cout << "  Transaction ID : "; set_color(CLR_WHITE); cout << json_get(r.body, "transaction_id") << "\n";
        set_color(CLR_CYAN); cout << "  Sent To        : "; set_color(CLR_WHITE); cout << recipient << "\n";
        set_color(CLR_CYAN); cout << "  Amount         : "; set_color(CLR_WHITE); cout << "KES " << fixed << setprecision(2) << amount << "\n";
        set_color(CLR_CYAN); cout << "  New Balance    : "; set_color(CLR_GREEN); cout << "KES " << json_get(r.body, "new_balance") << "\n";
        set_color(CLR_DEFAULT);
    } else {
        string err = json_get(r.body, "error");
        print_error(err.empty() ? "Transaction failed." : err);
    }
    press_enter();
}

// --- Feature: Deposit ---------------------------------------------------------
void do_deposit() {
    clear_screen(); print_header();
    set_color(CLR_WHITE); cout << "\n  === DEPOSIT MONEY ===\n\n"; set_color(CLR_DEFAULT);
    print_info("Simulate a cash deposit via M-Pesa agent");
    print_divider();

    double amount = get_amount("Deposit Amount (KES): ");
    if (amount <= 0) { print_error("Invalid amount."); press_enter(); return; }
    string ref = get_input("Reference (optional): ");

    cout << "\n"; print_info("Processing deposit...");

    ostringstream body;
    body << "{\"amount\":"        << fixed << setprecision(2) << amount
         << ",\"reference\":\"" << ref << "\"}";

    HttpResponse r = http_post("/api/deposit/", body.str(), true);
    if (r.status_code == 200) {
        print_success("Deposit successful!");
        cout << "\n";
        set_color(CLR_CYAN); cout << "  Transaction ID : "; set_color(CLR_WHITE); cout << json_get(r.body, "transaction_id") << "\n";
        set_color(CLR_CYAN); cout << "  Amount         : "; set_color(CLR_WHITE); cout << "KES " << fixed << setprecision(2) << amount << "\n";
        set_color(CLR_CYAN); cout << "  New Balance    : "; set_color(CLR_GREEN); cout << "KES " << json_get(r.body, "new_balance") << "\n";
        set_color(CLR_DEFAULT);
    } else {
        string err = json_get(r.body, "error");
        print_error(err.empty() ? "Deposit failed." : err);
    }
    press_enter();
}

// --- Feature: Withdraw --------------------------------------------------------
void do_withdraw() {
    clear_screen(); print_header();
    set_color(CLR_WHITE); cout << "\n  === WITHDRAW CASH ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    double amount = get_amount("Withdrawal Amount (KES): ");
    if (amount <= 0) { print_error("Invalid amount."); press_enter(); return; }
    string pin = get_hidden("Enter M-Pesa PIN: ");
    if (pin.empty()) { print_error("PIN is required."); press_enter(); return; }

    cout << "\n"; print_info("Processing withdrawal...");

    ostringstream body;
    body << "{\"amount\":"    << fixed << setprecision(2) << amount
         << ",\"pin\":\""     << pin   << "\""
         << ",\"description\":\"Cash withdrawal\"}";

    HttpResponse r = http_post("/api/withdraw/", body.str(), true);
    if (r.status_code == 200) {
        print_success("Withdrawal successful!");
        cout << "\n";
        set_color(CLR_CYAN); cout << "  Transaction ID : "; set_color(CLR_WHITE); cout << json_get(r.body, "transaction_id") << "\n";
        set_color(CLR_CYAN); cout << "  Amount         : "; set_color(CLR_WHITE); cout << "KES " << fixed << setprecision(2) << amount << "\n";
        set_color(CLR_CYAN); cout << "  New Balance    : "; set_color(CLR_GREEN); cout << "KES " << json_get(r.body, "new_balance") << "\n";
        set_color(CLR_DEFAULT);
    } else {
        string err = json_get(r.body, "error");
        print_error(err.empty() ? "Withdrawal failed." : err);
    }
    press_enter();
}

// --- Feature: Transaction History --------------------------------------------
void do_history() {
    clear_screen(); print_header();
    set_color(CLR_WHITE); cout << "\n  === TRANSACTION HISTORY (Last 10) ===\n\n"; set_color(CLR_DEFAULT);
    print_divider();

    HttpResponse r = http_get("/api/transactions/?limit=10");
    if (r.status_code != 200) {
        print_error("Failed to fetch transactions.");
        press_enter(); return;
    }

    set_color(CLR_CYAN); cout << "  Total: ";
    set_color(CLR_WHITE); cout << json_get(r.body, "count") << " transactions\n\n";

    set_color(CLR_YELLOW);
    cout << "  " << left << setw(10) << "TYPE"
                         << setw(14) << "AMOUNT(KES)"
                         << setw(14) << "BAL AFTER"
                         << "TRANSACTION ID\n";
    set_color(CLR_CYAN);
    cout << "  " << string(58, '-') << "\n";
    set_color(CLR_DEFAULT);

    string s = r.body;
    size_t pos = 0;
    int n = 0;
    while (n < 10) {
        size_t p = s.find("\"transaction_type\"", pos);
        if (p == string::npos) break;
        string sub    = s.substr(p, 500);
        string t_type = json_get(sub, "transaction_type");
        string t_amt  = json_get(sub, "amount");
        string t_bal  = json_get(sub, "balance_after");
        string t_id   = json_get(sub, "transaction_id");

        if (t_type == "SEND" || t_type == "WITHDRAW") set_color(CLR_RED);
        else set_color(CLR_GREEN);

        cout << "  " << left << setw(10) << t_type
                              << setw(14) << t_amt
                              << setw(14) << t_bal
                              << t_id << "\n";
        set_color(CLR_DEFAULT);
        pos = p + 1;
        n++;
    }
    if (n == 0) print_info("No transactions yet.");
    press_enter();
}

// --- Menus --------------------------------------------------------------------
void main_menu() {
    while (true) {
        clear_screen(); print_header();
        string name = g_session.full_name.empty() ? g_session.username : g_session.full_name;
        set_color(CLR_CYAN); cout << "\n  Logged in: ";
        set_color(CLR_WHITE); cout << name << "  [" << g_session.phone_number << "]\n\n";
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

        string c = get_input("Select option (1-6): ");
        if      (c == "1") do_balance();
        else if (c == "2") do_send();
        else if (c == "3") do_deposit();
        else if (c == "4") do_withdraw();
        else if (c == "5") do_history();
        else if (c == "6") { do_logout(); break; }
        else { print_error("Invalid option. Choose 1-6."); press_enter(); }
    }
}

void welcome_menu() {
    while (true) {
        clear_screen(); print_header();
        set_color(CLR_CYAN);    cout << "\n  Welcome to M-Pesa Terminal\n";
        set_color(CLR_DEFAULT); cout << "  Manage your M-Pesa account from the terminal.\n\n";
        print_divider();
        cout << "  [1]  Login\n";
        cout << "  [0]  Exit\n";
        print_divider();

        string c = get_input("Select option: ");
        if (c == "1") {
            do_login();
            if (g_session.logged_in) main_menu();
        } else if (c == "0") {
            clear_screen(); print_header();
            set_color(CLR_GREEN); cout << "\n  Thank you for using M-Pesa Terminal!\n\n";
            set_color(CLR_DEFAULT); break;
        } else {
            print_error("Invalid option.");
            press_enter();
        }
    }
}

// --- Entry point --------------------------------------------------------------
int main() {
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(CP_UTF8);
    welcome_menu();
    return 0;
}
