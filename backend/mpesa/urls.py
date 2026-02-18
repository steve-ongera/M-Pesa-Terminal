from django.urls import path
from .views import (
    BalanceView, SendMoneyView, DepositView,
    WithdrawView, TransactionHistoryView
)

urlpatterns = [
    path('balance/', BalanceView.as_view(), name='balance'),
    path('send/', SendMoneyView.as_view(), name='send_money'),
    path('deposit/', DepositView.as_view(), name='deposit'),
    path('withdraw/', WithdrawView.as_view(), name='withdraw'),
    path('transactions/', TransactionHistoryView.as_view(), name='transactions'),
]