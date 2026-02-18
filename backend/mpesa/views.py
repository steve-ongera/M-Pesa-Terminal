from rest_framework import status
from rest_framework.views import APIView
from rest_framework.response import Response
from rest_framework.permissions import AllowAny, IsAuthenticated
from rest_framework_simplejwt.tokens import RefreshToken
from rest_framework_simplejwt.authentication import JWTAuthentication
from django.contrib.auth import authenticate
from django.contrib.auth.hashers import check_password, make_password
from django.db import transaction as db_transaction
import uuid
import decimal

from .models import MpesaAccount, Transaction, BlacklistedToken
from .serializers import (
    LoginSerializer, MpesaAccountSerializer,
    TransactionSerializer, SendMoneySerializer,
    DepositSerializer, WithdrawSerializer
)


def generate_transaction_id():
    return f"TXN{uuid.uuid4().hex[:12].upper()}"


class LoginView(APIView):
    permission_classes = [AllowAny]

    def post(self, request):
        serializer = LoginSerializer(data=request.data)
        if not serializer.is_valid():
            return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

        username = serializer.validated_data['username']
        password = serializer.validated_data['password']

        user = authenticate(username=username, password=password)
        if not user:
            return Response(
                {'error': 'Invalid username or password'},
                status=status.HTTP_401_UNAUTHORIZED
            )

        if not user.is_active:
            return Response(
                {'error': 'Account is disabled'},
                status=status.HTTP_403_FORBIDDEN
            )

        # Check if user has mpesa account
        try:
            account = user.mpesa_account
        except MpesaAccount.DoesNotExist:
            return Response(
                {'error': 'No M-Pesa account linked to this user'},
                status=status.HTTP_404_NOT_FOUND
            )

        refresh = RefreshToken.for_user(user)
        return Response({
            'message': 'Login successful',
            'access': str(refresh.access_token),
            'refresh': str(refresh),
            'user': {
                'id': user.id,
                'username': user.username,
                'full_name': f"{user.first_name} {user.last_name}".strip(),
                'email': user.email,
                'phone_number': account.phone_number,
            }
        })


class LogoutView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request):
        try:
            refresh_token = request.data.get('refresh')
            if refresh_token:
                BlacklistedToken.objects.create(token=refresh_token)
                token = RefreshToken(refresh_token)
                token.blacklist()
            return Response({'message': 'Logged out successfully'})
        except Exception as e:
            return Response({'message': 'Logged out'}, status=status.HTTP_200_OK)


class BalanceView(APIView):
    permission_classes = [IsAuthenticated]

    def get(self, request):
        try:
            account = request.user.mpesa_account
            return Response({
                'phone_number': account.phone_number,
                'balance': str(account.balance),
                'currency': 'KES',
                'account_holder': f"{request.user.first_name} {request.user.last_name}".strip()
                    or request.user.username,
            })
        except MpesaAccount.DoesNotExist:
            return Response(
                {'error': 'M-Pesa account not found'},
                status=status.HTTP_404_NOT_FOUND
            )


class SendMoneyView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request):
        serializer = SendMoneySerializer(data=request.data)
        if not serializer.is_valid():
            return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

        data = serializer.validated_data

        try:
            sender_account = request.user.mpesa_account
        except MpesaAccount.DoesNotExist:
            return Response({'error': 'Sender account not found'}, status=status.HTTP_404_NOT_FOUND)

        # Verify PIN
        if not check_password(data['pin'], sender_account.pin):
            return Response({'error': 'Invalid PIN'}, status=status.HTTP_401_UNAUTHORIZED)

        amount = decimal.Decimal(str(data['amount']))

        if sender_account.balance < amount:
            return Response({'error': 'Insufficient balance'}, status=status.HTTP_400_BAD_REQUEST)

        # Find recipient
        try:
            recipient_account = MpesaAccount.objects.get(phone_number=data['recipient_phone'])
        except MpesaAccount.DoesNotExist:
            return Response(
                {'error': f"Recipient {data['recipient_phone']} not found"},
                status=status.HTTP_404_NOT_FOUND
            )

        if recipient_account == sender_account:
            return Response({'error': 'Cannot send money to yourself'}, status=status.HTTP_400_BAD_REQUEST)

        with db_transaction.atomic():
            txn_id = generate_transaction_id()
            sender_balance_before = sender_account.balance
            sender_account.balance -= amount
            sender_account.save()

            # Debit transaction
            Transaction.objects.create(
                account=sender_account,
                transaction_type='SEND',
                amount=amount,
                recipient_phone=data['recipient_phone'],
                description=data.get('description', ''),
                status='SUCCESS',
                transaction_id=txn_id,
                balance_before=sender_balance_before,
                balance_after=sender_account.balance,
            )

            # Credit recipient
            recv_balance_before = recipient_account.balance
            recipient_account.balance += amount
            recipient_account.save()

            recv_txn_id = generate_transaction_id()
            Transaction.objects.create(
                account=recipient_account,
                transaction_type='RECEIVE',
                amount=amount,
                recipient_phone=sender_account.phone_number,
                description=f"From {sender_account.phone_number}: {data.get('description', '')}",
                status='SUCCESS',
                transaction_id=recv_txn_id,
                balance_before=recv_balance_before,
                balance_after=recipient_account.balance,
            )

        return Response({
            'message': 'Money sent successfully',
            'transaction_id': txn_id,
            'amount': str(amount),
            'recipient': data['recipient_phone'],
            'new_balance': str(sender_account.balance),
            'currency': 'KES',
        })


class DepositView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request):
        serializer = DepositSerializer(data=request.data)
        if not serializer.is_valid():
            return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

        data = serializer.validated_data
        amount = decimal.Decimal(str(data['amount']))

        try:
            account = request.user.mpesa_account
        except MpesaAccount.DoesNotExist:
            return Response({'error': 'Account not found'}, status=status.HTTP_404_NOT_FOUND)

        with db_transaction.atomic():
            balance_before = account.balance
            account.balance += amount
            account.save()

            txn_id = generate_transaction_id()
            Transaction.objects.create(
                account=account,
                transaction_type='DEPOSIT',
                amount=amount,
                reference=data.get('reference', ''),
                description=f"Deposit via agent",
                status='SUCCESS',
                transaction_id=txn_id,
                balance_before=balance_before,
                balance_after=account.balance,
            )

        return Response({
            'message': 'Deposit successful',
            'transaction_id': txn_id,
            'amount': str(amount),
            'new_balance': str(account.balance),
            'currency': 'KES',
        })


class WithdrawView(APIView):
    permission_classes = [IsAuthenticated]

    def post(self, request):
        serializer = WithdrawSerializer(data=request.data)
        if not serializer.is_valid():
            return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

        data = serializer.validated_data

        try:
            account = request.user.mpesa_account
        except MpesaAccount.DoesNotExist:
            return Response({'error': 'Account not found'}, status=status.HTTP_404_NOT_FOUND)

        # Verify PIN
        if not check_password(data['pin'], account.pin):
            return Response({'error': 'Invalid PIN'}, status=status.HTTP_401_UNAUTHORIZED)

        amount = decimal.Decimal(str(data['amount']))

        if account.balance < amount:
            return Response({'error': 'Insufficient balance'}, status=status.HTTP_400_BAD_REQUEST)

        with db_transaction.atomic():
            balance_before = account.balance
            account.balance -= amount
            account.save()

            txn_id = generate_transaction_id()
            Transaction.objects.create(
                account=account,
                transaction_type='WITHDRAW',
                amount=amount,
                description=data.get('description', 'Cash withdrawal'),
                status='SUCCESS',
                transaction_id=txn_id,
                balance_before=balance_before,
                balance_after=account.balance,
            )

        return Response({
            'message': 'Withdrawal successful',
            'transaction_id': txn_id,
            'amount': str(amount),
            'new_balance': str(account.balance),
            'currency': 'KES',
        })


class TransactionHistoryView(APIView):
    permission_classes = [IsAuthenticated]

    def get(self, request):
        try:
            account = request.user.mpesa_account
        except MpesaAccount.DoesNotExist:
            return Response({'error': 'Account not found'}, status=status.HTTP_404_NOT_FOUND)

        limit = int(request.query_params.get('limit', 10))
        transactions = account.transactions.all()[:limit]
        serializer = TransactionSerializer(transactions, many=True)
        return Response({
            'count': account.transactions.count(),
            'transactions': serializer.data
        })