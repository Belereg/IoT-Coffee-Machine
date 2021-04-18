#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string.h>
#include <regex>


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

    // see beans level or refill beans container
    // Routes::Get(router, "/beans", Routes::bind(&CoffeeMachineController::getBeans, this));
    // Routes::Post(router, "/beans", Routes::bind(&CoffeeMachineController::setBeans, this));

    // see milk level or refill milk container
    // Routes::Get(router, "/milk", Routes::bind(&CoffeeMachineController::getMilk, this));
    // Routes::Post(router, "/mink", Routes::bind(&CoffeeMachineController::setMilk, this));

    // Clean coffee machine
    Routes::Get(router, "/getCleanLevel", Routes::bind(&CoffeeMachineController::cleanLevel, this));
    Routes::Post(router, "/cleanCoffeeMachine", Routes::bind(&CoffeeMachineController::clean, this));

    // Led strip controller
    Routes::Get(router, "/getLedStrip", Routes::bind(&CoffeeMachineController::LedStripState, this));
    Routes::Post(router, "/setLedStripState", Routes::bind(&CoffeeMachineController::LedStripSet, this));
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

  void makeCoffee(const Rest::Request &request, Http::ResponseWriter response)
  {
    // Very helpful -> https://kezunlin.me/post/f3c3eb8/

    // Need to explicitly use json::parse (not just = request.body()) or else it won't work :/
    json req = json::parse(request.body());
    cout << req.dump(4); //4 spaces as tab in json

    // Set coffee
    coffeeMachine.setCoffeeType(req["type"]);

    // Big mommy milky milkers can I drinky drink milky milk
    int milkLevel = coffeeMachine.getMilkLevel();
    milkLevel -= 10;
    coffeeMachine.setMilkLevel(milkLevel);

    // Create a json for response
    json res;
    res["status"] = "Coffee done :)";
    res["type"] = coffeeMachine.getCoffeeType();
    res["size"] = req["size"]; // I'm too lazy to do this now

    //int to string please kill me
    std::stringstream out;
    out << milkLevel;

    res["milk"] = out.str();

    //need to add this everytime
    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
    //send back json response
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

  void LedStripState(const Rest::Request &request, Http::ResponseWriter response)
  {
    json res;
    // We can see how dirty the coffee machine is before cleaning it
    string color;
    bool state = coffeeMachine.getLedStripState();
    if (state == false)
    {
      res["status"] = "LedStrip is off";
    }
    else 
    {
      
      color=coffeeMachine.getLedStripColor();
      res["status"]="LedStrip is on with color"+color;
      
    }
    

    //need to add this everytime
    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
    //send back json response
    response.send(Http::Code::Ok, res.dump(4));
  }


  void LedStripSet(const Rest::Request &request, Http::ResponseWriter response)
  {
    // Need to explicitly use json::parse (not just = request.body()) or else it won't work :/
    json req = json::parse(request.body());
    cout << req.dump(4); //4 spaces as tab in json

    string color=req["color"];
    string state = req["state"];

    json res;
    
    
    if (regex_match (state, regex("[0-1]") ) && regex_match (color, regex("#[a-zA-Z0-9]{6}") ))
    { 
          if (state.compare("0")==0)
          {
            coffeeMachine.setLedStripState(false);
            res["status"] = "LedStrip is off";
          }
          else 
          {
            coffeeMachine.setLedStripColor(color);
            res["status"]="LedStrip is on with color "+color;
          }
    }
    else
    {
      res["status"] = "Input invalid";
    }
    

    //need to add this everytime
    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
    //send back json response
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

    // DO SAME THING FOR REST OF ENUMS

    // ALSO WE COULD ADD SOME VALIDATIONS BEFORE SETTING BUT I DON'T KNOW IF WE SHOULD DO
    // THIS HERE IN THE SETTERS, AND RETURN 0 IF VALIDATION FAILED, OR DO IT IN ROUTE FUNCTIONS

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

    // LedStrip
    // Setter
    void setLedStripState(bool value)
    {
      ledStrip = value;
    }

    // Getter
    bool getLedStripState()
    {
      return ledStrip;
    }

    // LedStrip color
    // Setter
    void setLedStripColor(string value)
    {
      ledStripcolor = value;
    }

    // Getter
    string getLedStripColor()
    {
      return ledStripcolor;
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

    bool ledStrip = false;

    string ledStripcolor;
  };

  // Create the lock which prevents concurrent editing of the same variable
  using Lock = std::mutex;
  using Guard = std::lock_guard<Lock>;
  Lock coffeeMachineLock;

  // Instance of the microwave model
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