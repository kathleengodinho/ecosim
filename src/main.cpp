#define CROW_MAIN
#define CROW_STATIC_DIR "../public"
#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include "crow_all.h"
#include "json.hpp"
#include <random>

using json = nlohmann::json;

// Definir o tamanho da grade
const uint32_t NUM_ROWS = 15;
const uint32_t NUM_COLS = 15;

// Constants
const uint32_t PLANT_MAXIMUM_AGE = 10;
const uint32_t HERBIVORE_MAXIMUM_AGE = 50;
const uint32_t CARNIVORE_MAXIMUM_AGE = 80;
const uint32_t MAXIMUM_ENERGY = 200;
const uint32_t THRESHOLD_ENERGY_FOR_REPRODUCTION = 20;

// Mutex para controlar o acesso à grade de entidades
std::mutex gridMutex;


// Probabilities
const double PLANT_REPRODUCTION_PROBABILITY = 0.2;
const double HERBIVORE_REPRODUCTION_PROBABILITY = 0.075;
const double CARNIVORE_REPRODUCTION_PROBABILITY = 0.025;
const double HERBIVORE_MOVE_PROBABILITY = 0.7;
const double HERBIVORE_EAT_PROBABILITY = 0.9;
const double CARNIVORE_MOVE_PROBABILITY = 0.5;
const double CARNIVORE_EAT_PROBABILITY = 1.0;

// Type definitions
enum entity_type_t
{
    empty,
    plant,
    herbivore,
    carnivore
};

// Estrutura para representar uma posição na grade
struct pos_t
{
    uint32_t row;
    uint32_t col;
};

// Estrutura para representar uma entidade
struct entity_t
{
    entity_type_t type;
    int32_t energy;
    int32_t age;
    pos_t position;
};

// Grade que contém as entidades
std::vector<std::vector<entity_t>> entity_grid(NUM_ROWS, std::vector<entity_t>(NUM_COLS, { empty, 0, 0, {0, 0} }));

// Auxiliary code to convert the entity_type_t enum to a string
NLOHMANN_JSON_SERIALIZE_ENUM(entity_type_t, {
                                                {empty, " "},
                                                {plant, "P"},
                                                {herbivore, "H"},
                                                {carnivore, "C"},
                                            })

// Auxiliary code to convert the entity_t struct to a JSON object
namespace nlohmann
{
    void to_json(nlohmann::json &j, const entity_t &e)
    {
        j = nlohmann::json{{"type", e.type}, {"energy", e.energy}, {"age", e.age}};
    }
}


// Função para criar uma planta em uma posição aleatória na grade
void createPlant() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis_row(0, NUM_ROWS - 1);
    std::uniform_int_distribution<int> dis_col(0, NUM_COLS - 1);

    int row = dis_row(gen);
    int col = dis_col(gen);

    gridMutex.lock();
    if (entity_grid[row][col].type == empty) {
        entity_grid[row][col] = { plant, 0, 0, {row, col} };
    }
    gridMutex.unlock();
}

// Função para simular o comportamento das plantas
void plantBehavior() {
    while (true) {
        // Dormir por um curto período de tempo antes de cada iteração
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Ajuste conforme necessário

        // Verificar se uma nova planta deve ser criada com base na probabilidade de crescimento
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(0.0, 1.0);
        double probability = dis(gen);
        if (probability <= PLANT_REPRODUCTION_PROBABILITY) {
            createPlant();
        }
    }
}

int main()
{
    crow::SimpleApp app;

    // Endpoint to serve the HTML page
    CROW_ROUTE(app, "/")
    ([](crow::request &, crow::response &res)
     {
        // Return the HTML content here
        res.set_static_file_info_unsafe("../public/index.html");
        res.end(); });

    CROW_ROUTE(app, "/start-simulation")
        .methods("POST"_method)([](crow::request &req, crow::response &res)
                                { 

        // Limpar a grade de entidades
            gridMutex.lock();
            entity_grid.assign(NUM_ROWS, std::vector<entity_t>(NUM_COLS, { empty, 0, 0, {0, 0} }));
            gridMutex.unlock();

            // Parse do corpo da solicitação JSON
            json request_body = json::parse(req.body);

       // Validate the request body 
        uint32_t total_entinties = (uint32_t)request_body["plants"] + (uint32_t)request_body["herbivores"] + (uint32_t)request_body["carnivores"];
        if (total_entinties > NUM_ROWS * NUM_ROWS) {
        res.code = 400;
        res.body = "Too many entities";
        res.end();
        return;
        }

        // Clear the entity grid
        entity_grid.clear();
        entity_grid.assign(NUM_ROWS, std::vector<entity_t>(NUM_ROWS, { empty, 0, 0}));
        
        // Create the entities
        // <YOUR CODE HERE>

        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid; 
        res.body = json_grid.dump();
        res.end(); });

    // Endpoint to process HTTP GET requests for the next simulation iteration
    CROW_ROUTE(app, "/next-iteration")
        .methods("GET"_method)([](const crow::request &req)
                               {
        // Simulate the next iteration
        // Iterate over the entity grid and simulate the behaviour of each entity
        
        // <YOUR CODE HERE>
        // Limpar a grade de entidades
            gridMutex.lock();
            entity_grid.assign(NUM_ROWS, std::vector<entity_t>(NUM_COLS, { empty, 0, 0, {0, 0} }));
            gridMutex.unlock();

            // Parse do corpo da solicitação JSON
            json request_body = json::parse(req.body);

            // Criar plantas com base no número especificado
            int num_plants = request_body["plants"].get<int>();
            for (int i = 0; i < num_plants; ++i) {
                createPlant();
            }
        
         // Retornar a representação JSON da grade de entidades
          gridMutex.lock();
            json json_grid = entity_grid;
            gridMutex.unlock();
        return json_grid.dump(); });

    // Iniciar thread para simulação de plantas
         std::thread plantThread(plantBehavior);


    app.port(8080).run();
    // Esperar que a thread da planta termine
        plantThread.join();

    return 0;
}