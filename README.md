# IoT-Coffee-Machine

#### Building using Make

You can build the `CoffeeMachine` executable by running `make`.

#### Running

To start the server run\
`./CoffeeMachineController`

Your server should display the number of cores being used and no errors.

Now you can test the server by using curl or Postman (you can use our Postman collection).

#### Endpoints

POST `/coffee` - Make a coffee cup\
POST `/customCoffee` - Add a custom coffee with your own settings

GET `/getCleanLevel` - Check how clean your coffee maker is\
POST `/cleanCoffeeMachine` - Clean your coffee maker

GET `/getLedStrip` - See the state of your coffee machine's led strip\
POST `/setLedStrip` - Turn on/off or change led strip color

GET `/getResourceLevels` - Check your coffee machine's resources (water, milk, etc.)\
POST `/refillResourceLevel` - Refill water, milk, etc.
