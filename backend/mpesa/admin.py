from django.contrib import admin
from .models import MpesaAccount, Transaction, BlacklistedToken


@admin.register(MpesaAccount)
class MpesaAccountAdmin(admin.ModelAdmin):
    list_display = ['user', 'phone_number', 'balance', 'is_active', 'created_at']
    search_fields = ['user__username', 'phone_number']
    list_filter = ['is_active']


@admin.register(Transaction)
class TransactionAdmin(admin.ModelAdmin):
    list_display = ['transaction_id', 'account', 'transaction_type', 'amount', 'status', 'created_at']
    search_fields = ['transaction_id', 'account__phone_number']
    list_filter = ['transaction_type', 'status']
    readonly_fields = ['transaction_id', 'balance_before', 'balance_after', 'created_at']


@admin.register(BlacklistedToken)
class BlacklistedTokenAdmin(admin.ModelAdmin):
    list_display = ['blacklisted_at']
    readonly_fields = ['token', 'blacklisted_at']