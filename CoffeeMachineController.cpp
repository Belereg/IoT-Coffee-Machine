#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <algorithm>

#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>

#include <signal.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace Pistache;
using namespace nlohmann;

// Definition of the MicrowaveEnpoint class
class CoffeeMachineController
{
public:
  explicit CoffeeMachineController(Address addr)
      : httpEndpoint(std::make_shared<Http::Endpoint>(addr))
  {
  }

  // Initialization of the server. Additional options can be provided here
  void init(size_t thr = 2)
  {
    auto opts = Http::Endpoint::options()
                    .threads(static_cast<int>(thr));
    httpEndpoint->init(opts);
    // Server routes are loaded up
    setupRoutes();
  }

  // Server is started threaded.
  void start()
  {
    httpEndpoint->setHandler(router.handler());
    httpEndpoint->serveThreaded();
  }

  // When signaled server shuts down
  void stop()
  {
    httpEndpoint->shutdown();
  }

private:
  void setupRoutes()
  {
    using namespace Rest;
    Routes::Get(router, "/auth", Routes::bind(&CoffeeMachineController::doAuth, this));

    // I'm making the make coffee endpoint Post because it reads from request body and it alters the state of the machine. Sounds like post
    Routes::Post(router, "/coffee", Routes::bind(&CoffeeMachineController::makeCoffee, this));
    Routes::Post(router, "/customCoffee", Routes::bind(&CoffeeMachineController::setCustomRecipe, this));
    // Clean coffee machine
    Routes::Get(router, "/getCleanLevel", Routes::bind(&CoffeeMachineController::cleanLevel, this));
    Routes::Post(router, "/cleanCoffeeMachine", Routes::bind(&CoffeeMachineController::clean, this));

    // Refill resource levels
    Routes::Get(router, "/getResourceLevels", Routes::bind(&CoffeeMachineController::getRefillResourceLevels, this));
    Routes::Post(router, "/refillResourceLevel", Routes::bind(&CoffeeMachineController::refillResourceLevel, this));
  }

  void doAuth(const Rest::Request &request, Http::ResponseWriter response)
  {
    // Function that prints cookies
    // printCookies(request);
    // In the response object, it adds a cookie regarding the communications language.
    response.cookies()
        .add(Http::Cookie("lang", "en-US"));
    // Send the response
    response.send(Http::Code::Ok, "Coffee machine is online.");
  }
  string checkCoffeeReq(json req)
  { // Validates ingredients of a custom recipe request
    string status = "";

    if (!req["milkLevel"].is_number_integer() || !req["coffeeStrength"].is_number_integer() 
    || !req["beansLevel"].is_number_integer() || !req["waterLevel"].is_number_integer())
    {
      status = "Arguments for coffee details must be integer numbers!";
      return status;
    }
    int milkLevel = req["milkLevel"];
    int coffeeStrength = req["coffeeStrength"];
    int beansLevel = req["beansLevel"];
    int waterLevel = req["waterLevel"];

    if (milkLevel > 15 || milkLevel < 0)
      status.append("Invalid milk level! Milk level should be between 0 and 15!\n");
    if (coffeeStrength > 100 || coffeeStrength < 0)
      status.append("Invalid coffee level! Coffee level should be between 0 and 100!\n");
    if (beansLevel > 10 || beansLevel < 0)
      status.append("Invalid beans level! Beans level should be between 0 and 10!\n");
    if (waterLevel > 10 || waterLevel < 0)
      status.append("Invalid water level! Water level should be between 0 and 10!\n");

    if (status == "")
      status = "OK";
    return status;
  }
  void setCustomRecipe(const Rest::Request &request, Http::ResponseWriter response)
  {

    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
    json req = json::parse(request.body());
    json res;
    try
    {
      string status = checkCoffeeReq(req);
      if (status != "OK")
      {
        res["status"] = status;
        response.send(Http::Code::Bad_Request, res.dump(4));
        return;
      }
      int milkLevel = req["milkLevel"];
      int coffeeStrength = req["coffeeStrength"];
      int beansLevel = req["beansLevel"];
      int waterLevel = req["waterLevel"];

      vector<int> ingredients = {coffeeStrength, milkLevel, beansLevel, waterLevel};
      coffeeMachine.setCustomRecipe(ingredients);
      coffeeMachine.setCoffeeType("CUSTOM");

      res["status"] = "Added custom recipe!";
      response.send(Http::Code::Ok, res.dump(4));
      return;
    }
    catch (exception e)
    {
      res["status"] = "Creating recipe failed!";
      response.send(Http::Code::Bad_Request, res.dump(4));
    }
  }
  void makeCoffee(const Rest::Request &request, Http::ResponseWriter response)
  {
    // Very helpful -> https://kezunlin.me/post/f3c3eb8/

    // Need to explicitly use json::parse (not just = request.body()) or else it won't work :/
    json req = json::parse(request.body());
    cout << req.dump(4); //4 spaces as tab in json

    json res;
    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));

    //coffeeType validation
    try
    {
      if (!req["type"].is_string())
      {
        throw 505;
      }
      string type = req["type"];
      vector<string> coffeeTypeStrings = coffeeMachine.getCoffeeTypeValues();
      auto it = find(coffeeTypeStrings.begin(), coffeeTypeStrings.end(), type);
      if (it == coffeeTypeStrings.end())
      {
        throw 505;
      }
    }
    catch (int error)
    {
      res["status"] = "Invalid coffee type!";
      response.send(Http::Code::Bad_Request, res.dump(4));
      return;
    }

    //cupSize validation
    try
    {
      if (!req["cupSize"].is_string())
      {
        throw 505;
      }
      string cupSize = req["cupSize"];
      vector<string> cupSizeTypeStrings = coffeeMachine.getCupSizeValues();
      auto it = find(cupSizeTypeStrings.begin(), cupSizeTypeStrings.end(), cupSize);

      if (it == cupSizeTypeStrings.end())
      {
        throw 505;
      }
    }
    catch (int error)
    {
      res["status"] = "Invalid cup size!";
      response.send(Http::Code::Bad_Request, res.dump(4));
      return;
    }

    //foamSize validation
    try
    {
      if (!req["foamSize"].is_string())
      {
        throw 505;
      }
      string foamSize = req["foamSize"];
      vector<string> foamSizeTypeStrings = coffeeMachine.getFoamSizeValues();
      auto it = find(foamSizeTypeStrings.begin(), foamSizeTypeStrings.end(), foamSize);

      if (it == foamSizeTypeStrings.end())
      {
        throw 505;
      }
    }
    catch (int error)
    {
      res["status"] = "Invalid foam size!";
      response.send(Http::Code::Bad_Request, res.dump(4));
      return;
    }

    //coffeeStrength validation
    try
    {
      if (!req["coffeeStrength"].is_number_integer())
      {
        throw 505;
      }
      int coffeeStrength = req["coffeeStrength"];

      if (coffeeStrength < 45 || coffeeStrength > 100)
      {
        throw 505;
      }
    }
    catch (int error)
    {
      res["status"] = "Invalid coffee strength!";
      response.send(Http::Code::Bad_Request, res.dump(4));
      return;
    }

    string type = req["type"];
    string cupSize = req["cupSize"];
    int coffeeStrength = req["coffeeStrength"];
    string foamSize = req["foamSize"];

    int availableMilk = coffeeMachine.getMilkLevel();
    int availableWater = coffeeMachine.getWaterLevel();
    int availableBeans = coffeeMachine.getBeansLevel();
    int cleanLevel = coffeeMachine.getCleanLevel();

    json coffeeRecipes = coffeeMachine.getCoffeeRecipes();
    int req_milkLevel = int(coffeeRecipes[type][1]);
    int req_waterLevel = int(coffeeRecipes[type][2]);
    int req_beansLevel = int(coffeeRecipes[type][3]);

    if (availableMilk < req_milkLevel)
    { // Aici vin resursele custom de la featureul lui Samer
      res["statusMilk"] = "Not enough milk - Refill coffee machine!";
    }
    if (availableWater < req_waterLevel)
    { // Aici vin resursele custom de la featureul lui Samer
      res["statusWater"] = "Not enough water - Refill coffee machine!";
    }
    if (availableBeans < req_beansLevel)
    { // Aici vin resursele custom de la featureul lui Samer
      res["statusBeans"] = "Not enough beans - Refill coffee machine!";
    }
    if (cleanLevel <= 0)
    {
      res["statusClean"] = "Too dirty - Clean coffee machine!";
    }
    if (res.contains("statusMilk") || res.contains("statusWater") || res.contains("statusBeans") || res.contains("statusClean"))
    {
      response.send(Http::Code::Bad_Request, res.dump(4));
      return;
    }

    // Set coffee
    coffeeMachine.setCoffeeType(type);
    coffeeMachine.setCupSize(cupSize);
    coffeeMachine.setFoamSize(foamSize);
    coffeeMachine.setCoffeeStrength(coffeeStrength);

    availableMilk -= req_milkLevel; 
    coffeeMachine.setMilkLevel(availableMilk);

    availableWater -= req_waterLevel; 
    coffeeMachine.setWaterLevel(availableWater);

    availableBeans -= req_beansLevel; 
    coffeeMachine.setBeansLevel(availableBeans);

    cleanLevel -= 5;
    coffeeMachine.setCleanLevel(cleanLevel);

    // Fill json for response
    res["type"] = coffeeMachine.getCoffeeType();
    res["cupSize"] = coffeeMachine.getCupSize();
    res["coffeeStrength"] = coffeeMachine.getCoffeeStrength();
    res["foamSize"] = coffeeMachine.getFoamSize();
    res["status"] = "Coffee done :)";

    // All good - send the coffee
    response.send(Http::Code::Ok, res.dump(4));
  }

  void cleanLevel(const Rest::Request &request, Http::ResponseWriter response)
  {
    json res;

    // We can see how dirty the coffee machine is before cleaning it
    int cleanLevel = coffeeMachine.getCleanLevel();
    if (cleanLevel < 10)
    {
      res["status"] = "Super dirty - cannot make coffee until cleaned";
    }
    else if (cleanLevel < 30 && cleanLevel >= 10)
    {
      res["status"] = "Dirty - will need cleaning soon";
    }
    else if (cleanLevel < 70 && cleanLevel >= 30)
    {
      res["status"] = "Ok - does not need cleaning";
    }
    else if (cleanLevel <= 100 && cleanLevel >= 70)
    {
      res["status"] = "Clean and in good order";
    }

    //need to add this everytime
    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
    //send back json response
    response.send(Http::Code::Ok, res.dump(4));
  }

  void clean(const Rest::Request &request, Http::ResponseWriter response)
  {
    // We can see how dirty the coffee machine is before cleaning it
    int cleanLevel = coffeeMachine.getCleanLevel();
    json res;
    if (cleanLevel < 70)
    {
      coffeeMachine.setCleanLevel(100);
      res["status"] = "Your coffee machine was cleaned";
    }
    else
    {
      res["status"] = "Your coffee machine does not need to be cleaned";
    }
    // Create a json for response
    res["cleanLevel"] = coffeeMachine.getCleanLevel();

    //need to add this everytime
    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
    //send back json response
    response.send(Http::Code::Ok, res.dump(4));
  }

  void getRefillResourceLevels(const Rest::Request &request, Http::ResponseWriter response)
  {
    json res;

    int milkLevel = coffeeMachine.getMilkLevel();
    int waterLevel = coffeeMachine.getWaterLevel();
    int beansLevel = coffeeMachine.getBeansLevel();

    res["milkLevel"] = "Milk level: " + to_string(milkLevel) + "%";
    res["waterLevel"] = "Water level : " + to_string(waterLevel) + " %";
    res["beansLevel"] = "Beans level : " + to_string(beansLevel) + " %";

    res["status"] = (milkLevel < 30 || waterLevel < 30 || beansLevel < 30) ? "One or more resource levels need a refill" : "Resource levels are good";

    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
    response.send(Http::Code::Ok, res.dump(4));
  }

  void refillResourceLevel(const Rest::Request &request, Http::ResponseWriter response)
  {
    json req = json::parse(request.body());
    cout << req.dump(4); //4 spaces as tab in json

    json res;

    try
    {
      if (!req["resourceType"].is_string())
      {
        throw 505;
      }

      string resourceType = req["resourceType"];

      if (resourceType == "MILK")
      {
        if (coffeeMachine.getMilkLevel() > 99)
        {
          res["status"] = "Milk level is already full.";
        }
        else
        {
          coffeeMachine.setMilkLevel(100);
          res["status"] = "Milk level has been refilled.";
        }
      }
      else if (resourceType == "WATER")
      {
        if (coffeeMachine.getWaterLevel() > 99)
        {
          res["status"] = "Water level is already full.";
        }
        else
        {
          coffeeMachine.setWaterLevel(100);
          res["status"] = "Water level has been refilled.";
        }
      }
      else if (resourceType == "BEANS")
      {
        if (coffeeMachine.getBeansLevel() > 99)
        {
          res["status"] = "Beans level is already full.";
        }
        else
        {
          coffeeMachine.setBeansLevel(100);
          res["status"] = "Beans level has been refilled.";
        }
      }
      else
      {
        throw 505;
      }
    }
    catch (int error)
    {
      res["status"] = "Invalid resource type!";
      response.send(Http::Code::Bad_Request, res.dump(4));
      return;
    }

    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
    response.send(Http::Code::Ok, res.dump(4));
  }

  // Defining the class of the CoffeeMachine. It should model the entire configuration of the CoffeeMachine
  class CoffeeMachine
  {
  public:
    explicit CoffeeMachine() {}

    // COFFEE TYPE
    // Setter
    void setCoffeeType(string value)
    {
      // Find index of coffee type string
      auto it = find(coffeeTypeString.begin(), coffeeTypeString.end(), value);

      if (it != coffeeTypeString.end())
      {                                               // If found
        int index = it - coffeeTypeString.begin();    // Get index
        coffeeType = static_cast<COFFEE_TYPE>(index); // Int to enum
      }
      // We could return 0 or 1 depending if enum was found and set
    }

    // Getter
    string getCoffeeType()
    {
      return coffeeTypeString[coffeeType];
    }

    // CUP SIZE
    // Setter
    void setCupSize(string value)
    {
      // Find index of coffee type string
      auto it = find(cupSizeString.begin(), cupSizeString.end(), value);

      if (it != cupSizeString.end())
      {                                         // If found
        int index = it - cupSizeString.begin(); // Get index
        cupSize = static_cast<CUP_SIZE>(index); // Int to enum
      }
      // We could return 0 or 1 depending if enum was found and set
    }

    // Getter
    string getCupSize()
    {
      return cupSizeString[cupSize];
    }

    // FOAM SIZE
    // Setter
    void setFoamSize(string value)
    {
      // Find index of coffee type string
      auto it = find(foamSizeString.begin(), foamSizeString.end(), value);

      if (it != foamSizeString.end())
      {                                           // If found
        int index = it - foamSizeString.begin();  // Get index
        foamSize = static_cast<FOAM_SIZE>(index); // Int to enum
      }
      // We could return 0 or 1 depending if enum was found and set
    }

    // Getter
    string getFoamSize()
    {
      return foamSizeString[foamSize];
    }

    // COFFEE STRENGTH
    // Setter
    void setCoffeeStrength(int value)
    {
      coffeeStrength = value;
    }

    // Getter
    int getCoffeeStrength()
    {
      return coffeeStrength;
    }

    // MILK
    // Setter
    void setMilkLevel(int value)
    {
      milkLevel = value;
    }

    // Getter
    int getMilkLevel()
    {
      return milkLevel;
    }

    // WATER
    // Setter
    void setWaterLevel(int value)
    {
      waterLevel = value;
    }

    // Getter
    int getWaterLevel()
    {
      return waterLevel;
    }

    // BEANS
    // Setter
    void setBeansLevel(int value)
    {
      beansLevel = value;
    }

    // Getter
    int getBeansLevel()
    {
      return beansLevel;
    }

    // Clean
    // Setter
    void setCleanLevel(int value)
    {
      cleanLevel = value;
    }

    // Getter
    int getCleanLevel()
    {
      return cleanLevel;
    }

    vector<string> getCoffeeTypeValues()
    {
      return coffeeTypeString;
    }

    vector<string> getCupSizeValues()
    {
      return cupSizeString;
    }

    vector<string> getFoamSizeValues()
    {
      return foamSizeString;
    }

    vector<string> getResourceTypeValues()
    {
      return resourceTypeString;
    }
    json getCoffeeRecipes()
    {
      return coffeeRecipes;
    }
    void setCustomRecipe(vector<int> ingredients)
    {
      coffeeTypeString.push_back("CUSTOM");
      coffeeRecipes["CUSTOM"] = ingredients;
    }

  private:
    // Defining and instantiating settings.
    enum COFFEE_TYPE
    {
      CAPPUCCINO,
      ESPRESSO,
      LATTE_MACHIATTO,
      CAFFE_LATTE,
      DOPPIO,
      AMERICANO
    } coffeeType = COFFEE_TYPE::CAFFE_LATTE;

    // Can't find another easy way to convert string to enum and back so I'm gonna use this
    vector<string> coffeeTypeString =
        {"CAPPUCCINO", "ESPRESSO", "LATTE_MACHIATTO", "CAFFE_LATTE", "DOPPIO", "AMERICANO"};

    vector<string> resourceTypeString =
        {"MILK", "WATER", "BEANS"};

    //Enum names are in global scope so they must be unique => cant have CUP_SIZE::S and FOAM_SIZE::S
    enum CUP_SIZE
    {
      CUP_S,
      CUP_M,
      CUP_L,
      CUP_XL
    } cupSize = CUP_SIZE::CUP_S;

    vector<string> cupSizeString =
        {"CUP_S", "CUP_M", "CUP_L", "CUP_XL"};

    enum FOAM_SIZE
    {
      FOAM_S,
      FOAM_M,
      FOAM_L
    } foamSize = FOAM_SIZE::FOAM_S;

    vector<string> foamSizeString =
        {"FOAM_S", "FOAM_M", "FOAM_L"};

    int coffeeStrength = 45; // 45mg - 100mg

    int milkLevel = 100; // 0 - 100

    int waterLevel = 100; // 0 - 100

    int beansLevel = 100; // 0 - 100

    int cleanLevel = 100; // 0 - 100
    json coffeeRecipes = {
        {"CAPPUCCINO", {50, 5, 10, 5}},
        {"ESPRESSO", {100, 0, 10, 5}},
        {"LATTE_MACHIATTO", {50, 10, 10, 5}},
        {"DOPPIO", {100, 0, 7, 10}},
        {"AMERICANO", {60, 8, 7, 5}}};
  };

  // Create the lock which prevents concurrent editing of the same variable
  using Lock = std::mutex;
  using Guard = std::lock_guard<Lock>;
  Lock coffeeMachineLock;

  // Instance of the microwave model // I think you mean coffee machine model
  CoffeeMachine coffeeMachine;

  // Defining the httpEndpoint and a router.
  std::shared_ptr<Http::Endpoint> httpEndpoint;
  Rest::Router router;
};

int main(int argc, char *argv[])
{

  // This code is needed for gracefull shutdown of the server when no longer needed.
  sigset_t signals;
  if (sigemptyset(&signals) != 0 || sigaddset(&signals, SIGTERM) != 0 || sigaddset(&signals, SIGINT) != 0 || sigaddset(&signals, SIGHUP) != 0 || pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0)
  {
    perror("install signal handler failed");
    return 1;
  }

  // Set a port on which your server to communicate
  Port port(9080);

  // Number of threads used by the server
  int thr = 2;

  if (argc >= 2)
  {
    port = static_cast<uint16_t>(std::stol(argv[1]));

    if (argc == 3)
      thr = std::stoi(argv[2]);
  }

  Address addr(Ipv4::any(), port);

  cout << "Cores = " << hardware_concurrency() << endl;
  cout << "Using " << thr << " threads" << endl;

  // Instance of the class that defines what the server can do.
  CoffeeMachineController stats(addr);

  // Initialize and start the server
  stats.init(thr);
  stats.start();

  // Code that waits for the shutdown sinal for the server
  int signal = 0;
  int status = sigwait(&signals, &signal);
  if (status == 0)
  {
    std::cout << "received signal " << signal << std::endl;
  }
  else
  {
    std::cerr << "sigwait returns " << status << std::endl;
  }

  stats.stop();
}