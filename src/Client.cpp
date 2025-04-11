#include "Client.h"
#include <iostream>

std::mutex Client::coutMutex;

void Client::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << "[" << name << "] " << message << std::endl;
}

void Client::onOrderPlaced(OrderId orderId, Price price, Amount amount) {
    log("Order placed - ID: " + std::to_string(orderId.value) + 
        ", Price: " + std::to_string(price.value) + 
        ", Amount: " + std::to_string(amount.value));
}

void Client::onOrderCanceled(OrderId orderId, int reasonCode) {
    log("Order canceled - ID: " + std::to_string(orderId.value) + 
        ", Reason: " + std::to_string(reasonCode));
}

void Client::onOrderTraded(OrderId orderId, Price price, Amount amount) {
    log("Order traded - ID: " + std::to_string(orderId.value) + 
        ", Price: " + std::to_string(price.value) + 
        ", Amount: " + std::to_string(amount.value));
} 