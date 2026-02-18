# 💚 M-Pesa Terminal — Django + C++ Project

A full-stack M-Pesa simulation with:
- **Django REST Framework** backend (Python)
- **C++ terminal client** communicating over HTTP using libcurl

---

## 📁 Project Structure

```
mpesa_project/
├── backend/                   ← Django REST API
│   ├── manage.py
│   ├── requirements.txt
│   ├── setup.py               ← Run once to init DB + demo users
│   ├── mpesa_backend/         ← Django project settings
│   │   ├── settings.py
│   │   └── urls.py
│   └── mpesa/             ← Main app
│       ├── models.py          ← MpesaAccount, Transaction, BlacklistedToken
│       ├── views.py           ← All API endpoints
│       ├── serializers.py
│       ├── urls.py
│       └── admin.py
└── cpp_client/
    ├── mpesa_client.cpp       ← C++ terminal client
    └── Makefile
```

---

## 🚀 Setup — Django Backend

### 1. Install Python dependencies
```bash
cd backend
pip install -r requirements.txt
```

### 2. Initialize database and demo users
```bash
python setup.py
```

This creates:
| User   | Password      | Phone       | PIN  | Balance    |
|--------|---------------|-------------|------|------------|
| john   | password123   | 0712345678  | 1234 | KES 5,000  |
| jane   | password123   | 0722345678  | 1234 | KES 2,000  |
| admin  | admin123      | —           | —    | admin only |

### 3. Start the server
```bash
python manage.py runserver 0.0.0.0:8000
```

Admin panel: http://localhost:8000/admin/

---

## 🖥️ Setup — C++ Terminal Client

### 1. Install libcurl
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev

# macOS
brew install curl
```

### 2. Compile
```bash
cd cpp_client
make
```

Or manually:
```bash
g++ -o mpesa_client mpesa_client.cpp -lcurl -std=c++17
```

### 3. Run
```bash
./mpesa_client
```

---

## 🔌 API Endpoints

| Method | Endpoint                  | Auth | Description              |
|--------|---------------------------|------|--------------------------|
| POST   | `/api/auth/login/`        | No   | Login, returns JWT       |
| POST   | `/api/auth/logout/`       | Yes  | Invalidate token         |
| POST   | `/api/auth/refresh/`      | No   | Refresh JWT token        |
| GET    | `/api/balance/`           | Yes  | Get M-Pesa balance       |
| POST   | `/api/send/`              | Yes  | Send money               |
| POST   | `/api/deposit/`           | Yes  | Deposit funds            |
| POST   | `/api/withdraw/`          | Yes  | Withdraw cash            |
| GET    | `/api/transactions/`      | Yes  | Transaction history      |

---

## 📋 API Request/Response Examples

### Login
```json
POST /api/auth/login/
{ "username": "john", "password": "password123" }

Response:
{
  "access": "<jwt_token>",
  "refresh": "<refresh_token>",
  "user": {
    "username": "john",
    "full_name": "John Doe",
    "phone_number": "0712345678"
  }
}
```

### Send Money
```json
POST /api/send/
Authorization: Bearer <token>
{
  "recipient_phone": "0722345678",
  "amount": 500,
  "pin": "1234",
  "description": "Lunch money"
}
```

### Withdraw
```json
POST /api/withdraw/
Authorization: Bearer <token>
{ "amount": 1000, "pin": "1234" }
```

---

## 🎨 C++ Terminal Features

- Colored terminal UI (ANSI escape codes)
- Hidden PIN entry (stty -echo)
- JWT session management (login / logout)
- Real-time balance checking
- Send money by phone number
- Deposit simulation
- Cash withdrawal with PIN
- Transaction history (last 10)

---

## 🔒 Security Notes

- Passwords are hashed via Django's PBKDF2
- PINs are hashed via Django's PBKDF2
- JWT tokens expire after 8 hours
- Logout blacklists the refresh token
- All money operations require PIN verification

---

## 📈 Extending the Project

- Add M-Pesa STK Push integration (real Safaricom API)
- Add OTP verification via SMS (Africa's Talking / Twilio)
- Add transaction receipts (PDF generation)
- Add multiple account tiers / limits
- Swap SQLite for PostgreSQL in production