#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>

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

    //...etc, same get and post routes for each setting, health, water, whatever
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
    //Very helpful -> https://kezunlin.me/post/f3c3eb8/

    // Need to explicitly use json::parse (not just = request.body()) or else it won't work :/
    json req = json::parse(request.body());
    cout << req.dump(4); //4 spaces as tab in json

    json res;
    //i'll copy everything from request and add a status
    res["status"] = "Coffee done :)";
    res["type"] = req["type"];
    res["size"] = req["size"];

    int milkLevel = coffeeMachine.getMilkLevel();
    // Big mommy milky milkers can I drinky drink milky milk
    milkLevel -= 10;
    coffeeMachine.setMilkLevel(milkLevel);


    //int to string please kill me
    std::stringstream out;
    out << milkLevel;

    res["milk"] = out.str();

    //need to add this everytime
    response.headers().add<Pistache::Http::Header::ContentType>(MIME(Application, Json));
    //send back json response
    response.send(Http::Code::Ok, res.dump(4));
  }

  // Endpoint to configure one of the CoffeeMachine's settings.
  // void setBeans(const Rest::Request& request, Http::ResponseWriter response){
  //
  // }

  // Setting to get the settings value of one of the configurations of the CoffeeMachine
  // void getBeans(const Rest::Request& request, Http::ResponseWriter response){

  // }

  // Defining the class of the CoffeeMachine. It should model the entire configuration of the CoffeeMachine
  class CoffeeMachine
  {
  public:
    explicit CoffeeMachine() {}

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

  private:
    // Defining and instantiating settings.
    enum COFFEE_TYPE
    {
      CAPUCCINO,
      ESPRESSO,
      LATTE_MACHIATTO,
      DOPPIO,
      AMERICANO
    } coffeeType = COFFEE_TYPE::CAPUCCINO;

    enum CUP_SIZE
    {
      S,
      M,
      L,
      XL
    } cupSize = CUP_SIZE::S;

    int milkLevel = 100;
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