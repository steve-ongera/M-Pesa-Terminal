/**
 * ============================================
 *   M-PESA Terminal Client - Java Edition
 *   Connects to Django REST backend
 * ============================================
 *
 *  HOW TO COMPILE & RUN:
 *  ─────────────────────────────────────────────
 *  Java 8+ is required. No external libraries needed.
 *  Uses java.net.HttpURLConnection (built-in).
 *
 *  Compile:
 *    javac MpesaClient.java
 *
 *  Run:
 *    java MpesaClient
 *
 *  ─────────────────────────────────────────────
 *  Make sure Django backend is running first:
 *    python manage.py runserver 0.0.0.0:8000
 *  ─────────────────────────────────────────────
 *
 *  Demo credentials:
 *    Username: john  Password: password123  PIN: 1234
 *    Username: jane  Password: password123  PIN: 1234
 * ============================================
 */

import java.io.*;
import java.net.*;
import java.util.Scanner;

public class MpesaClient {

    // ─── Server config ────────────────────────────────────────────────────────
    private static final String BASE_URL = "http://127.0.0.1:8000";

    // ─── ANSI color codes (work on Windows 10+, Linux, macOS) ────────────────
    private static final String RESET  = "\033[0m";
    private static final String BOLD   = "\033[1m";
    private static final String RED    = "\033[1;31m";
    private static final String GREEN  = "\033[1;32m";
    private static final String YELLOW = "\033[1;33m";
    private static final String CYAN   = "\033[1;36m";
    private static final String WHITE  = "\033[1;37m";

    // ─── Session state ────────────────────────────────────────────────────────
    private static String accessToken   = "";
    private static String refreshToken  = "";
    private static String username      = "";
    private static String fullName      = "";
    private static String phoneNumber   = "";
    private static boolean loggedIn     = false;

    // ─── Input scanner ────────────────────────────────────────────────────────
    private static final Scanner scanner = new Scanner(System.in);

    // =========================================================================
    //  HTTP HELPERS
    // =========================================================================

    static class HttpResponse {
        int    statusCode;
        String body;
        HttpResponse(int code, String body) {
            this.statusCode = code;
            this.body = body;
        }
    }

    private static HttpResponse httpRequest(String method, String path,
                                             String jsonBody, boolean useAuth) {
        try {
            URL url = new URL(BASE_URL + path);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod(method);
            conn.setRequestProperty("Content-Type", "application/json");
            conn.setRequestProperty("Accept", "application/json");
            conn.setConnectTimeout(8000);
            conn.setReadTimeout(8000);

            if (useAuth && !accessToken.isEmpty()) {
                conn.setRequestProperty("Authorization", "Bearer " + accessToken);
            }

            if (jsonBody != null && !jsonBody.isEmpty()) {
                conn.setDoOutput(true);
                try (OutputStream os = conn.getOutputStream()) {
                    os.write(jsonBody.getBytes("UTF-8"));
                }
            }

            int statusCode;
            try {
                statusCode = conn.getResponseCode();
            } catch (IOException e) {
                return new HttpResponse(0, "{\"error\":\"Cannot connect to server\"}");
            }

            // Read success or error stream
            InputStream stream = (statusCode >= 200 && statusCode < 300)
                    ? conn.getInputStream()
                    : conn.getErrorStream();

            StringBuilder sb = new StringBuilder();
            if (stream != null) {
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(stream, "UTF-8"))) {
                    String line;
                    while ((line = reader.readLine()) != null) sb.append(line);
                }
            }

            conn.disconnect();
            return new HttpResponse(statusCode, sb.toString());

        } catch (Exception e) {
            return new HttpResponse(0, "{\"error\":\"" + e.getMessage() + "\"}");
        }
    }

    private static HttpResponse httpPost(String path, String body, boolean auth) {
        return httpRequest("POST", path, body, auth);
    }

    private static HttpResponse httpGet(String path) {
        return httpRequest("GET", path, null, true);
    }

    // =========================================================================
    //  JSON HELPER  (no external library — simple key extraction)
    // =========================================================================

    private static String jsonGet(String json, String key) {
        if (json == null || json.isEmpty()) return "";
        String search = "\"" + key + "\":";
        int pos = json.indexOf(search);
        if (pos == -1) return "";
        pos += search.length();

        // Skip whitespace
        while (pos < json.length() && json.charAt(pos) == ' ') pos++;
        if (pos >= json.length()) return "";

        if (json.charAt(pos) == '"') {
            // String value
            pos++;
            StringBuilder result = new StringBuilder();
            while (pos < json.length() && json.charAt(pos) != '"') {
                if (json.charAt(pos) == '\\' && pos + 1 < json.length()) pos++;
                result.append(json.charAt(pos++));
            }
            return result.toString();
        } else {
            // Number / bool
            int end = pos;
            while (end < json.length() && ",}\n]".indexOf(json.charAt(end)) == -1) end++;
            return json.substring(pos, end).trim();
        }
    }

    // =========================================================================
    //  UI HELPERS
    // =========================================================================

    private static void clearScreen() {
        // Works on Windows (cmd), Linux, macOS
        try {
            if (System.getProperty("os.name").contains("Windows")) {
                new ProcessBuilder("cmd", "/c", "cls").inheritIO().start().waitFor();
            } else {
                System.out.print("\033[2J\033[H");
                System.out.flush();
            }
        } catch (Exception e) {
            // Fallback: print blank lines
            for (int i = 0; i < 40; i++) System.out.println();
        }
    }

    private static void printHeader() {
        System.out.println(CYAN);
        System.out.println("  +==========================================+");
        System.out.println("  |         M-PESA TERMINAL  v1.0            |");
        System.out.println("  |         Java Edition - Django API        |");
        System.out.println("  +==========================================+");
        System.out.print(RESET);
    }

    private static void printDivider() {
        System.out.println(CYAN + "  ------------------------------------------" + RESET);
    }

    private static void printSuccess(String msg) {
        System.out.println(GREEN + "  [OK]    " + msg + RESET);
    }

    private static void printError(String msg) {
        System.out.println(RED + "  [ERROR] " + msg + RESET);
    }

    private static void printInfo(String msg) {
        System.out.println(YELLOW + "  [INFO]  " + msg + RESET);
    }

    private static void pressEnter() {
        System.out.print(CYAN + "\n  Press Enter to continue..." + RESET);
        scanner.nextLine();
    }

    private static String getInput(String prompt) {
        System.out.print(YELLOW + "  " + prompt + WHITE);
        String input = scanner.nextLine();
        System.out.print(RESET);
        return input.trim();
    }

    // Hidden password input — masks with *
    // Uses Console if available (proper terminal), falls back to plain Scanner
    private static String getHidden(String prompt) {
        System.out.print(YELLOW + "  " + prompt + RESET);
        Console console = System.console();
        if (console != null) {
            // Real terminal: Console.readPassword() hides input
            char[] chars = console.readPassword();
            return chars != null ? new String(chars) : "";
        } else {
            // IDE / redirected: fall back to visible input with warning
            System.out.print(YELLOW + "(visible - run from cmd for hidden input) " + WHITE);
            String input = scanner.nextLine();
            System.out.print(RESET);
            return input.trim();
        }
    }

    private static double getAmount(String prompt) {
        String input = getInput(prompt);
        try {
            return Double.parseDouble(input);
        } catch (NumberFormatException e) {
            return -1.0;
        }
    }

    // =========================================================================
    //  FEATURES
    // =========================================================================

    // ─── Login ────────────────────────────────────────────────────────────────
    private static void doLogin() {
        clearScreen();
        printHeader();
        System.out.println(WHITE + "\n  === LOGIN ===" + RESET);
        System.out.println();
        printDivider();

        String user = getInput("Username: ");
        String pass = getHidden("Password: ");

        if (user.isEmpty() || pass.isEmpty()) {
            printError("Username and password cannot be empty.");
            pressEnter();
            return;
        }

        System.out.println();
        printInfo("Connecting to M-Pesa server...");

        String body = "{\"username\":\"" + user + "\",\"password\":\"" + pass + "\"}";
        HttpResponse r = httpPost("/api/auth/login/", body, false);

        if (r.statusCode == 0) {
            printError("Cannot reach server. Run: python manage.py runserver");
            pressEnter();
            return;
        }

        if (r.statusCode == 200) {
            accessToken  = jsonGet(r.body, "access");
            refreshToken = jsonGet(r.body, "refresh");
            username     = user;
            fullName     = jsonGet(r.body, "full_name");
            phoneNumber  = jsonGet(r.body, "phone_number");
            loggedIn     = true;

            String name = fullName.isEmpty() ? username : fullName;
            printSuccess("Welcome, " + name + "!");
            printSuccess("Phone: " + phoneNumber);
        } else {
            String err = jsonGet(r.body, "error");
            printError(err.isEmpty() ? "Login failed (HTTP " + r.statusCode + ")" : err);
        }
        pressEnter();
    }

    // ─── Logout ───────────────────────────────────────────────────────────────
    private static void doLogout() {
        httpPost("/api/auth/logout/", "{\"refresh\":\"" + refreshToken + "\"}", true);
        accessToken = refreshToken = username = fullName = phoneNumber = "";
        loggedIn = false;

        clearScreen();
        printHeader();
        printSuccess("Logged out successfully. Goodbye!");
        pressEnter();
    }

    // ─── Balance ──────────────────────────────────────────────────────────────
    private static void doBalance() {
        clearScreen();
        printHeader();
        System.out.println(WHITE + "\n  === M-PESA BALANCE ===" + RESET);
        System.out.println();
        printDivider();

        HttpResponse r = httpGet("/api/balance/");

        if (r.statusCode == 200) {
            String balance = jsonGet(r.body, "balance");
            String phone   = jsonGet(r.body, "phone_number");
            String holder  = jsonGet(r.body, "account_holder");

            System.out.println();
            System.out.println(CYAN  + "  Account Holder : " + WHITE + holder + RESET);
            System.out.println(CYAN  + "  Phone Number   : " + WHITE + phone  + RESET);
            System.out.println();
            System.out.println(GREEN + "  +================================+" + RESET);
            System.out.println(GREEN + "  |  Available Balance             |" + RESET);
            System.out.printf( GREEN + "  |  KES %-26s" + GREEN + "|\n" + RESET, WHITE + balance + GREEN);
            System.out.println(GREEN + "  +================================+" + RESET);
        } else {
            String err = jsonGet(r.body, "error");
            printError(err.isEmpty() ? "Failed to get balance." : err);
        }
        pressEnter();
    }

    // ─── Send Money ───────────────────────────────────────────────────────────
    private static void doSend() {
        clearScreen();
        printHeader();
        System.out.println(WHITE + "\n  === SEND MONEY ===" + RESET);
        System.out.println();
        printDivider();

        String recipient = getInput("Recipient Phone (e.g. 0722345678): ");
        if (recipient.isEmpty()) { printError("Phone is required."); pressEnter(); return; }

        double amount = getAmount("Amount (KES): ");
        if (amount <= 0) { printError("Invalid amount."); pressEnter(); return; }

        String desc = getInput("Description (optional, Enter to skip): ");
        String pin  = getHidden("Enter M-Pesa PIN: ");
        if (pin.isEmpty()) { printError("PIN is required."); pressEnter(); return; }

        System.out.println();
        printInfo("Processing transaction...");

        String body = String.format(
            "{\"recipient_phone\":\"%s\",\"amount\":%.2f,\"pin\":\"%s\",\"description\":\"%s\"}",
            recipient, amount, pin, desc
        );

        HttpResponse r = httpPost("/api/send/", body, true);

        if (r.statusCode == 200) {
            printSuccess("Money sent successfully!");
            System.out.println();
            System.out.println(CYAN + "  Transaction ID : " + WHITE + jsonGet(r.body, "transaction_id") + RESET);
            System.out.println(CYAN + "  Sent To        : " + WHITE + recipient + RESET);
            System.out.printf (CYAN + "  Amount         : " + WHITE + "KES %.2f\n" + RESET, amount);
            System.out.println(CYAN + "  New Balance    : " + GREEN + "KES " + jsonGet(r.body, "new_balance") + RESET);
        } else {
            String err = jsonGet(r.body, "error");
            printError(err.isEmpty() ? "Transaction failed." : err);
        }
        pressEnter();
    }

    // ─── Deposit ──────────────────────────────────────────────────────────────
    private static void doDeposit() {
        clearScreen();
        printHeader();
        System.out.println(WHITE + "\n  === DEPOSIT MONEY ===" + RESET);
        printInfo("Simulate a cash deposit via M-Pesa agent");
        System.out.println();
        printDivider();

        double amount = getAmount("Deposit Amount (KES): ");
        if (amount <= 0) { printError("Invalid amount."); pressEnter(); return; }

        String ref = getInput("Reference (optional): ");

        System.out.println();
        printInfo("Processing deposit...");

        String body = String.format(
            "{\"amount\":%.2f,\"reference\":\"%s\"}",
            amount, ref
        );

        HttpResponse r = httpPost("/api/deposit/", body, true);

        if (r.statusCode == 200) {
            printSuccess("Deposit successful!");
            System.out.println();
            System.out.println(CYAN + "  Transaction ID : " + WHITE + jsonGet(r.body, "transaction_id") + RESET);
            System.out.printf (CYAN + "  Amount         : " + WHITE + "KES %.2f\n" + RESET, amount);
            System.out.println(CYAN + "  New Balance    : " + GREEN + "KES " + jsonGet(r.body, "new_balance") + RESET);
        } else {
            String err = jsonGet(r.body, "error");
            printError(err.isEmpty() ? "Deposit failed." : err);
        }
        pressEnter();
    }

    // ─── Withdraw ─────────────────────────────────────────────────────────────
    private static void doWithdraw() {
        clearScreen();
        printHeader();
        System.out.println(WHITE + "\n  === WITHDRAW CASH ===" + RESET);
        System.out.println();
        printDivider();

        double amount = getAmount("Withdrawal Amount (KES): ");
        if (amount <= 0) { printError("Invalid amount."); pressEnter(); return; }

        String pin = getHidden("Enter M-Pesa PIN: ");
        if (pin.isEmpty()) { printError("PIN is required."); pressEnter(); return; }

        System.out.println();
        printInfo("Processing withdrawal...");

        String body = String.format(
            "{\"amount\":%.2f,\"pin\":\"%s\",\"description\":\"Cash withdrawal\"}",
            amount, pin
        );

        HttpResponse r = httpPost("/api/withdraw/", body, true);

        if (r.statusCode == 200) {
            printSuccess("Withdrawal successful!");
            System.out.println();
            System.out.println(CYAN + "  Transaction ID : " + WHITE + jsonGet(r.body, "transaction_id") + RESET);
            System.out.printf (CYAN + "  Amount         : " + WHITE + "KES %.2f\n" + RESET, amount);
            System.out.println(CYAN + "  New Balance    : " + GREEN + "KES " + jsonGet(r.body, "new_balance") + RESET);
        } else {
            String err = jsonGet(r.body, "error");
            printError(err.isEmpty() ? "Withdrawal failed." : err);
        }
        pressEnter();
    }

    // ─── Transaction History ──────────────────────────────────────────────────
    private static void doHistory() {
        clearScreen();
        printHeader();
        System.out.println(WHITE + "\n  === TRANSACTION HISTORY (Last 10) ===" + RESET);
        System.out.println();
        printDivider();

        HttpResponse r = httpGet("/api/transactions/?limit=10");

        if (r.statusCode != 200) {
            printError("Failed to fetch transactions.");
            pressEnter();
            return;
        }

        System.out.println(CYAN + "  Total: " + WHITE + jsonGet(r.body, "count") + " transactions\n" + RESET);

        // Table header
        System.out.printf(YELLOW + "  %-10s %-14s %-14s %s\n" + RESET,
            "TYPE", "AMOUNT(KES)", "BAL AFTER", "TRANSACTION ID");
        System.out.println(CYAN + "  " + "-".repeat(58) + RESET);

        // Parse each transaction
        String body = r.body;
        int pos = 0;
        int shown = 0;

        while (shown < 10) {
            int p = body.indexOf("\"transaction_type\"", pos);
            if (p == -1) break;

            String sub   = body.substring(p, Math.min(p + 500, body.length()));
            String tType = jsonGet(sub, "transaction_type");
            String tAmt  = jsonGet(sub, "amount");
            String tBal  = jsonGet(sub, "balance_after");
            String tId   = jsonGet(sub, "transaction_id");

            String color = (tType.equals("SEND") || tType.equals("WITHDRAW")) ? RED : GREEN;

            System.out.printf(color + "  %-10s %-14s %-14s %s\n" + RESET,
                tType, tAmt, tBal, tId);

            pos = p + 1;
            shown++;
        }

        if (shown == 0) printInfo("No transactions yet.");
        pressEnter();
    }

    // =========================================================================
    //  MENUS
    // =========================================================================

    private static void mainMenu() {
        while (true) {
            clearScreen();
            printHeader();

            String name = fullName.isEmpty() ? username : fullName;
            System.out.println(CYAN + "\n  Logged in: " + WHITE + name +
                               "  [" + phoneNumber + "]" + RESET);
            System.out.println();
            System.out.println(WHITE + "  MAIN MENU" + RESET);
            printDivider();
            System.out.println("  [1]  Check Balance");
            System.out.println("  [2]  Send Money");
            System.out.println("  [3]  Deposit");
            System.out.println("  [4]  Withdraw");
            System.out.println("  [5]  Transaction History");
            System.out.println("  [6]  Logout");
            printDivider();

            String choice = getInput("Select option (1-6): ");

            switch (choice) {
                case "1": doBalance(); break;
                case "2": doSend();    break;
                case "3": doDeposit(); break;
                case "4": doWithdraw(); break;
                case "5": doHistory(); break;
                case "6": doLogout();  return;
                default:
                    printError("Invalid option. Choose 1-6.");
                    pressEnter();
            }
        }
    }

    private static void welcomeMenu() {
        while (true) {
            clearScreen();
            printHeader();
            System.out.println(CYAN + "\n  Welcome to M-Pesa Terminal (Java)" + RESET);
            System.out.println("  Manage your M-Pesa account from the terminal.");
            System.out.println();
            printDivider();
            System.out.println("  [1]  Login");
            System.out.println("  [0]  Exit");
            printDivider();

            String choice = getInput("Select option: ");

            switch (choice) {
                case "1":
                    doLogin();
                    if (loggedIn) mainMenu();
                    break;
                case "0":
                    clearScreen();
                    printHeader();
                    System.out.println(GREEN + "\n  Thank you for using M-Pesa Terminal!\n" + RESET);
                    return;
                default:
                    printError("Invalid option.");
                    pressEnter();
            }
        }
    }

    // =========================================================================
    //  ENTRY POINT
    // =========================================================================

    public static void main(String[] args) {
        // Enable ANSI colors on Windows 10+ terminal
        // (Works automatically in cmd.exe and Windows Terminal on Win10+)
        welcomeMenu();
        scanner.close();
    }
}