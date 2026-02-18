from rest_framework import serializers
from django.contrib.auth.models import User
from .models import MpesaAccount, Transaction


class UserSerializer(serializers.ModelSerializer):
    class Meta:
        model = User
        fields = ['id', 'username', 'email', 'first_name', 'last_name']


class MpesaAccountSerializer(serializers.ModelSerializer):
    user = UserSerializer(read_only=True)

    class Meta:
        model = MpesaAccount
        fields = ['id', 'user', 'phone_number', 'balance', 'is_active', 'created_at']
        read_only_fields = ['balance']


class TransactionSerializer(serializers.ModelSerializer):
    class Meta:
        model = Transaction
        fields = [
            'id', 'transaction_type', 'amount', 'recipient_phone',
            'reference', 'description', 'status', 'transaction_id',
            'balance_before', 'balance_after', 'created_at'
        ]
        read_only_fields = [
            'status', 'transaction_id', 'balance_before', 'balance_after', 'created_at'
        ]


class SendMoneySerializer(serializers.Serializer):
    recipient_phone = serializers.CharField(max_length=15)
    amount = serializers.DecimalField(max_digits=12, decimal_places=2, min_value=1)
    pin = serializers.CharField(max_length=10, write_only=True)
    description = serializers.CharField(max_length=200, required=False, default='')


class DepositSerializer(serializers.Serializer):
    amount = serializers.DecimalField(max_digits=12, decimal_places=2, min_value=1)
    reference = serializers.CharField(max_length=100, required=False, default='')


class WithdrawSerializer(serializers.Serializer):
    amount = serializers.DecimalField(max_digits=12, decimal_places=2, min_value=1)
    pin = serializers.CharField(max_length=10, write_only=True)
    description = serializers.CharField(max_length=200, required=False, default='')


class LoginSerializer(serializers.Serializer):
    username = serializers.CharField()
    password = serializers.CharField(write_only=True)