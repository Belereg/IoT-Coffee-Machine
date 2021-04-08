CoffeeMachineController: CoffeeMachineController.cpp
	g++ $< -o $@ -lpistache -lcrypto -lssl -lpthread