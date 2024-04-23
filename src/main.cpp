#define CROW_MAIN
#define CROW_STATIC_DIR "../public"

#include "crow_all.h"
#include "json.hpp"
#include <random>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <condition_variable>

static const uint32_t NUM_ROWS = 15;

std::mutex grid;
std::condition_variable cv_thread;
std::condition_variable cv_grid;
std::mutex mtx_cv;
std::mutex mtx_wait_end;
std::mutex mtx_counter;
std::mutex mtx_total;
std::mutex mtx_cout;
int total_entidades;
int *iteracao_terminada = new int(0);
int *thread_criada = new int(0);

// Constants
const uint32_t PLANT_MAXIMUM_AGE = 10;
const uint32_t HERBIVORE_MAXIMUM_AGE = 50;
const uint32_t CARNIVORE_MAXIMUM_AGE = 80;
const uint32_t MAXIMUM_ENERGY = 200;
const uint32_t THRESHOLD_ENERGY_FOR_REPRODUCTION = 20;

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
    carnivore,
    morta
};

struct pos_t
{
    uint32_t i;
    uint32_t j;
};

struct entity_t
{
    entity_type_t type;
    int32_t energy;
    int32_t age;
};

// Auxiliary code to convert the entity_type_t enum to a string
NLOHMANN_JSON_SERIALIZE_ENUM(entity_type_t, {
                                                {empty, " "},
                                                {plant, "P"},
                                                {herbivore, "H"},
                                                {carnivore, "C"},
                                                {morta, "M"},
                                            })

// Auxiliary code to convert the entity_t struct to a JSON object
namespace nlohmann
{
    void to_json(nlohmann::json &j, const entity_t &e)
    {
        j = nlohmann::json{{"type", e.type}, {"energy", e.energy}, {"age", e.age}};
    }
}

// Grid that contains the entities
static std::vector<std::vector<entity_t>> entity_grid;
std::vector<std::thread> thread_vec;

bool getProbability(double prob)
{
    double probPercentage = prob * 100;
    int random_number = rand() % 100;
    return random_number < probPercentage;
}
int randomNumber()  //utilizado para gerar uma direção aleatoria 
{
    int number = rand() % NUM_ROWS;
    return number;
} 

void simulatePlant(int i, int j)
{
    std::unique_lock<std::mutex> lock(mtx_cv);// mutex para coordenar a entrada das threads, para que não cogestione (entre mais de uma de uma vez) 

    while (true)
    {
        cv_thread.wait(lock); // Aguarda o sinal para começar a simulação

        // Verifica se a planta está morta ou atingiu a idade máxima
        if (entity_grid[i][j].type == morta || entity_grid[i][j].age >= PLANT_MAXIMUM_AGE)
        {
            // Remove a planta e decrementa o contador de entidades
            entity_grid[i][j].type = empty;
            entity_grid[i][j].age = 0;
            mtx_total.lock(); // mutex para garantir a exclusã mutua ao total_entidades 
            total_entidades--;
            mtx_total.unlock();
            break; // sai do loop
        }

        //calcula a probabilidade de reprodução da planta 
        bool prob = getProbability(PLANT_REPRODUCTION_PROBABILITY);
        
        // Se a planta decidir se reproduzir
        if (prob)
        {
            grid.lock();// Mutex para acessar a grade de entidades

            // Gera uma direção aleatória
            int direction = randomNumber() % 4; // 0: cima, 1: baixo, 2: esquerda, 3: direita
            
            // Verifica se a célula adjacente na direção escolhida está vazia e atualiza-a para uma nova planta
            if (direction=0 && i + 1 < NUM_ROWS && entity_grid[i + 1][j].type == empty)
            {
                mtx_cout.lock(); // Mutex para evitar a mistura de saída no console
                std::cout << "New plant i " << i + 1 << " j " << j << std::endl;
                mtx_cout.unlock();
                entity_grid[i + 1][j].type = plant;
            }
            else if (direction=1  && j + 1 < NUM_ROWS && entity_grid[i][j + 1].type == empty)
            {
                mtx_cout.lock();// Mutex para evitar a mistura de saída no console
                std::cout << "New plant i " << i << " j " << j + 1 << std::endl;
                mtx_cout.unlock();
                entity_grid[i][j + 1].type = plant;
            }
            else if (direction=2 && j - 1 > 0 && entity_grid[i][j - 1].type == empty)
            {
                mtx_cout.lock(); // Mutex para evitar a mistura de saída no console
                std::cout << "New plant i " << i << " j " << j - 1 << std::endl;
                mtx_cout.unlock();
                entity_grid[i][j - 1].type = plant;
            }
            else if ( direction=3 && i - 1 > 0 && entity_grid[i - 1][j].type == empty)
            {
                mtx_cout.lock(); // Mutex para evitar a mistura de saída no console
                std::cout << "New plant i " << i - 1 << " j " << j << std::endl;
                mtx_cout.unlock();
                entity_grid[i - 1][j].type = plant;
            }
            grid.unlock(); // Libera o acesso à grade de entidades
        }
        entity_grid[i][j].age++; // Incrementa a idade da planta

        mtx_counter.lock(); // Mutex para acessar e modificar iteracao_terminada
        (*iteracao_terminada)++;  // Atualiza o contador de iterações concluídas
        std::cout << "Iteração terminada: i " << i << " j " << j << std::endl;
        cv_grid.notify_all(); // Notifica todas as threads aguardando na condição cv_grid
        mtx_counter.unlock(); // Libera o acesso a iteracao_terminada
    }
}

void simulateHerbivore(int i, int j)
{
    std::unique_lock<std::mutex> lock(mtx_cv); // Mutex para coordenar a entrada das threads

    while (true)
    {
        cv_thread.wait(lock);// Aguarda o sinal para começar a simulação

         // Verificar se o herbívoro está vivo, atingiu a idade máxima ou tem energia zero
        if (entity_grid[i][j].type == morta || entity_grid[i][j].age >= HERBIVORE_MAXIMUM_AGE || entity_grid[i][j].energy <= 0)
        {
            // Remove o herbívoro, reiniciando suas propriedades, e decrementa o contador de entidades
            entity_grid[i][j].type = empty;
            entity_grid[i][j].age = 0;
            entity_grid[i][j].energy = 0;
            mtx_total.lock();// Mutex para garantir a exclusão mútua ao modificar total_entidades
            total_entidades--;
            mtx_total.unlock();
            break;
        }

        // Movimento do herbivoro
        if (getProbability(HERBIVORE_MOVE_PROBABILITY))
        {
            grid.lock();// Mutex para acessar a grade de entidade

            // Procurar uma célula adjacente vazia ou com planta para se mover
            int direction = randomNumber() % 4; // 0: cima, 1: baixo, 2: esquerda, 3: direita
            int newRow = i, newCol = j;

            if (direction == 0 && i > 0 && (entity_grid[i - 1][j].type == empty || entity_grid[i - 1][j].type == plant))
            {
                newRow = i - 1;
                std::cout << "i-1" << (entity_grid[i - 1][j].type == empty) << std::endl;
            }
            else if (direction == 1 && i < NUM_ROWS - 1 && (entity_grid[i + 1][j].type == empty || entity_grid[i + 1][j].type == plant))
            {
                newRow = i + 1;
                std::cout << "i+1" << (entity_grid[i + 1][j].type == empty) << std::endl;
            }
            else if (direction == 2 && j > 0 && (entity_grid[i][j - 1].type == empty || entity_grid[i][j - 1].type == plant))
            {
                newCol = j - 1;
                std::cout << "j-1" << (entity_grid[i][j - 1].type == empty) << std::endl;
            }
            else if (direction == 3 && j < NUM_ROWS - 1 && (entity_grid[i][j + 1].type == empty || entity_grid[i][j + 1].type == plant))
            {
                newCol = j + 1;
                std::cout << "j+1" << (entity_grid[i][j + 1].type == empty) << std::endl;
            }

            // verifica se houve movimento
            if (newRow != i || newCol != j)
            {
                // Se a nova célula contém uma planta, comer a planta
                if (entity_grid[newRow][newCol].type == plant)
                {
                    // Comer a planta
                    entity_grid[i][j].energy += 30;
                    if (entity_grid[i][j].energy > 100)
                        entity_grid[i][j].energy = 100; // Limitar a energia a 100 unidades
                    entity_grid[newRow][newCol] = {empty, 0, 0}; // Remover a planta
                }
                // Mover o herbívoro para a nova célula
                entity_grid[newRow][newCol] = {herbivore, entity_grid[i][j].energy - 5, 0}; // Mover o herbívoro
                entity_grid[i][j] = {empty, 0, 0};                                          // Deixar a celula anterior vazia
            }
            else
            {
                // Se não houve movimento, decrementar a energia do herbivoro
                entity_grid[i][j].energy -= 5; // Custo de energia para movimento sem sucesso
            }
            grid.unlock();
        }

        // Alimentação do herbívoro
        if (getProbability(HERBIVORE_EAT_PROBABILITY))
        {
            grid.lock();//mutex para acessaar a grade de entidades 

            // Verificar se há uma planta adjacente para comer
            if (i > 0 && entity_grid[i - 1][j].type == plant)
            {
                // comer a planta e atualizar a energia do herbivoro 
                entity_grid[i - 1][j] = {empty, 0, 0}; // Remover a planta
                entity_grid[i][j].energy += 30; 
                if (entity_grid[i][j].energy > 100)
                    entity_grid[i][j].energy = 100; // Limitar a energia a 100 unidades      // Ganhar energia ao comer a planta
            }
            else if (i < NUM_ROWS - 1 && entity_grid[i + 1][j].type == plant)
            {
                entity_grid[i + 1][j] = {empty, 0, 0}; // Remover a planta
                entity_grid[i][j].energy += 30; 
                if (entity_grid[i][j].energy > 100)
                    entity_grid[i][j].energy = 100; // Limitar a energia a 100 unidades       // Ganhar energia ao comer a planta
            }
            else if (j > 0 && entity_grid[i][j - 1].type == plant)
            {
                entity_grid[i][j - 1] = {empty, 0, 0}; // Remover a planta
                entity_grid[i][j].energy += 30; 
                if (entity_grid[i][j].energy > 100)
                    entity_grid[i][j].energy = 100; // Limitar a energia a 100 unidades       // Ganhar energia ao comer a planta
            }
            else if (j < NUM_ROWS - 1 && entity_grid[i][j + 1].type == plant)
            {
                entity_grid[i][j + 1] = {empty, 0, 0}; // Remover a planta
                entity_grid[i][j].energy += 30;      
                if (entity_grid[i][j].energy > 100)
                    entity_grid[i][j].energy = 100; // Limitar a energia a 100 unidades  // Ganhar energia ao comer a planta
            }
            grid.unlock(); //libera o acesso a grade de entidades 
        }

        // Reprodução do herbívoro
        if (entity_grid[i][j].energy > THRESHOLD_ENERGY_FOR_REPRODUCTION && getProbability(HERBIVORE_REPRODUCTION_PROBABILITY))
        {
            grid.lock(); //mutex para acessar a grade de entidades

            // Procurar uma célula adjacente vazia para colocar a prole
            int direction = randomNumber() % 4; // 0: cima, 1: baixo, 2: esquerda, 3: direita
            int newRow = i, newCol = j;

            if (direction == 0 && i > 0 && entity_grid[i - 1][j].type == empty)
            {
                newRow = i - 1;
                std::cout << "i-1" << (entity_grid[i - 1][j].type == empty) << std::endl;
            }
            else if (direction == 1 && i < NUM_ROWS - 1 && entity_grid[i + 1][j].type == empty)
            {
                newRow = i + 1;
                std::cout << "i+1" << (entity_grid[i + 1][j].type == empty) << std::endl;
            }
            else if (direction == 2 && j > 0 && entity_grid[i][j - 1].type == empty)
            {
                newCol = j - 1;
                std::cout << "j-1" << (entity_grid[i][j - 1].type == empty) << std::endl;
            }
            else if (direction == 3 && j < NUM_ROWS - 1 && entity_grid[i][j + 1].type == empty)
            {
                newCol = j + 1;
                std::cout << "j+1" << (entity_grid[i][j + 1].type == empty) << std::endl;
            }

            // Colocar na célula vazia adjacente
            entity_grid[newRow][newCol] = {herbivore, entity_grid[i][j].energy - 10, 0}; //onde subtraii a energia do herbivoro caso ele mexa 
            entity_grid[i][j].energy -= 10; //decrementa a energia do herbivoro caso atenda as necessidades 
            grid.unlock(); // libera o acesso á grade de entidades 
        }

        // Incrementar a idade do herbívoro
        entity_grid[i][j].age++;

        mtx_counter.lock(); // mutex para acessar e modificar iteração_terminada 
        (*iteracao_terminada)++; // atualiza o contador de iterações concluidas 
        std::cout << "Iteração terminada H: i " << i << " j " << j << std::endl;
        cv_grid.notify_all(); // notifica todas as threads aguardando na condição cv_grid
        mtx_counter.unlock(); //libera o acesso a iteracao_terminada 
    }
}

void simulateCarnivore(int i, int j)
{
    std::unique_lock<std::mutex> lock(mtx_cv); // mutex para coordenar a entrada das threads 
    while (true)
    {
        cv_thread.wait(lock); // aguarda o sinal para começar a simulação

        // Verificar se o carnívoro está vivo
        if (entity_grid[i][j].type == morta || entity_grid[i][j].age >= CARNIVORE_MAXIMUM_AGE || entity_grid[i][j].energy <= 0)
        {
            //remove se o carnivoro, reinicia suas propriedades e decrementa o contador de entidades 
            entity_grid[i][j].type = empty;
            entity_grid[i][j].age = 0;
            entity_grid[i][j].energy = 0;
            mtx_total.lock(); //mutex para garantir a exclusao mutua do total_entidade 
            total_entidades--;
            mtx_total.unlock();
            break;
        }

        // Movimento do carnívoro
        if (getProbability(CARNIVORE_MOVE_PROBABILITY))
        {
            // Procurar uma célula adjacente vazia ou com herbívoro para se mover
            int direction = randomNumber() % 4; // 0: cima, 1: baixo, 2: esquerda, 3: direita
            int newRow = i, newCol = j;
            grid.lock();// mutex para acessar a grade de entidades 
            if (direction == 0 && i > 0 && (entity_grid[i - 1][j].type == empty || entity_grid[i - 1][j].type == herbivore))
            {
                newRow = i - 1;
            }
            else if (direction == 1 && i < NUM_ROWS - 1 && (entity_grid[i + 1][j].type == empty || entity_grid[i + 1][j].type == herbivore))
            {
                newRow = i + 1;
            }
            else if (direction == 2 && j > 0 && (entity_grid[i][j - 1].type == empty || entity_grid[i][j - 1].type == herbivore))
            {
                newCol = j - 1;
            }
            else if (direction == 3 && j < NUM_ROWS - 1 && (entity_grid[i][j + 1].type == empty || entity_grid[i][j + 1].type == herbivore))
            {
                newCol = j + 1;
            }

            // Verifica se houve movimento
            if (newRow != i || newCol != j)
            {
                // Mover o carnívoro para a nova célula
                entity_grid[newRow][newCol] = {carnivore, entity_grid[i][j].energy - 5, 0}; // Mover o carnívoro
                entity_grid[i][j] = {empty, 0, 0};  
                 if (entity_grid[i][j].energy > 100)
                        entity_grid[i][j].energy = 100;  //limitar a energia                                      // Deixar a célula anterior vazia
            }
            else
            {
                // Se não houve movimento, decrementar a energia do carnívoro
                entity_grid[i][j].energy -= 5; // Custo de energia para movimento sem sucesso
            }
            grid.unlock(); // libera o acesso a grade de entidades
        }

        // Alimentação do carnívoro
        if (getProbability(CARNIVORE_EAT_PROBABILITY))
        {
            grid.lock();
            // Verificar se há um herbívoro adjacente para comer
            if (i > 0 && entity_grid[i - 1][j].type == herbivore)
            {
                entity_grid[i - 1][j] = {empty, 0, 0}; // Remover o herbívoro
                entity_grid[i][j].energy += 20;       // Ganhar energia ao comer o herbívoro
                 if (entity_grid[i][j].energy > 100)
                        entity_grid[i][j].energy = 100;
            }
            else if (i < NUM_ROWS - 1 && entity_grid[i + 1][j].type == herbivore)
            {
                entity_grid[i + 1][j] = {empty, 0, 0}; // Remover o herbívoro
                entity_grid[i][j].energy += 20;        // Ganhar energia ao comer o herbívoro
                 if (entity_grid[i][j].energy > 100)
                        entity_grid[i][j].energy = 100;
            }
            else if (j > 0 && entity_grid[i][j - 1].type == herbivore)
            {
                entity_grid[i][j - 1] = {empty, 0, 0}; // Remover o herbívoro
                entity_grid[i][j].energy += 20;        // Ganhar energia ao comer o herbívoro
                 if (entity_grid[i][j].energy > 100)
                        entity_grid[i][j].energy = 100;
            }
            else if (j < NUM_ROWS - 1 && entity_grid[i][j + 1].type == herbivore)
            {
                entity_grid[i][j + 1] = {empty, 0, 0}; // Remover o herbívoro
                entity_grid[i][j].energy += 20;        // Ganhar energia ao comer o herbívoro
                 if (entity_grid[i][j].energy > 100)
                        entity_grid[i][j].energy = 100;
            }
            grid.unlock(); //libera o acesso a grade de entidades 
        }

        // Reprodução do carnívoro
        if (entity_grid[i][j].energy > THRESHOLD_ENERGY_FOR_REPRODUCTION && getProbability(CARNIVORE_REPRODUCTION_PROBABILITY))
        {
            // Procurar uma célula adjacente vazia para colocar a prole
            int direction = randomNumber() % 4; // 0: cima, 1: baixo, 2: esquerda, 3: direita
            int newRow = i, newCol = j;
            grid.lock();
            if (direction == 0 && i > 0 && entity_grid[i - 1][j].type == empty)
            {
                newRow = i - 1;
            }
            else if (direction == 1 && i < NUM_ROWS - 1 && entity_grid[i + 1][j].type == empty)
            {
                newRow = i + 1;
            }
            else if (direction == 2 && j > 0 && entity_grid[i][j - 1].type == empty)
            {
                newCol = j - 1;
            }
            else if (direction == 3 && j < NUM_ROWS - 1 && entity_grid[i][j + 1].type == empty)
            {
                newCol = j + 1;
            }

            // Colocar na célula vazia adjacente
            entity_grid[newRow][newCol] = {carnivore, entity_grid[i][j].energy - 10, 0};
            entity_grid[i][j].energy -= 10;
            grid.unlock(); //libera o acesso a grade de entidades
        }

        // Incrementar a idade do carnívoro
        entity_grid[i][j].age++;

        mtx_counter.lock(); // mutex para acessar e modificr iteacao_termiinada
        (*iteracao_terminada)++; // atualiza o contador de iteracoes concluidas 
        std::cout << "Iteração terminada c: i " << i << " j " << j << std::endl;
        cv_grid.notify_all();// notifica todas as threads guardando na cv_grid
        mtx_counter.unlock(); // libera o acesso a iteracao_terminada 
    }
}

int main()
{
    srand(time(NULL)); // Inicialização do seed do gerador de números aleatórios
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
        // Parse the JSON request body
        nlohmann::json request_body = nlohmann::json::parse(req.body);

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

        // Simula a próxima iteração
        // Itera sobre a grade de entidades e simula o comportamento de cada entidade

        for (int i = 0; i < (uint32_t)request_body["plants"]; i++) {
            entity_t newPlant;
            newPlant.age = 0;
            newPlant.type = plant;
            int foundPos = 0;
            int row;
            int col;
            std::cout << i << std::endl;
            while (foundPos == 0){
                row = randomNumber();
                col = randomNumber();
                std::cout << entity_grid[row][col].type << std::endl;
                if(entity_grid[row][col].type == empty) {
                    entity_grid[row][col] = newPlant;
                    foundPos = 1;
                }
            }
        }

        for (int i = 0; i < (uint32_t)request_body["carnivores"]; i++) {
            entity_t newCarnivore;
            newCarnivore.age = 0;
            newCarnivore.type = carnivore;
            newCarnivore.energy = 100;
            int foundPos = 0;
            int row;
            int col;
            while (foundPos == 0){
                row = randomNumber();
                col = randomNumber();
                if(entity_grid[row][col].type == empty) {
                    entity_grid[row][col] = newCarnivore;
                    foundPos = 1;
                }
            }
        }

        for (int i = 0; i < (uint32_t)request_body["herbivores"]; i++) {
            entity_t newHerbivore;
            newHerbivore.age = 0;
            newHerbivore.type = herbivore;
            newHerbivore.energy = 100;
            int foundPos = 0;
            int row;
            int col;
            while (foundPos == 0){
                row = randomNumber();
                col = randomNumber();
                if(entity_grid[row][col].type == empty) {
                    entity_grid[row][col] = newHerbivore;
                    foundPos = 1;
                }
            }
        }
        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid;
        res.body = json_grid.dump();
        res.end(); });

    // Endpoint to process HTTP GET requests for the next simulation iteration
    CROW_ROUTE(app, "/next-iteration")
        .methods("GET"_method)([]()
                               {
        // Simulate the next iteration
        // Iterate over the entity grid and simulate the behaviour of each entity

    
        // Limpa o contador de iterações terminadas
        mtx_counter.lock();
        *iteracao_terminada = 0;
        mtx_counter.unlock(); 

        for (int i = 0; i < NUM_ROWS; i++) {
            for (int j = 0; j < NUM_ROWS; j++) {
                if (entity_grid[i][j].type == plant && entity_grid[i][j].age == 0) {
                    // mtx_cout.lock();
                    // std::cout << "Plant detected at position: (" << i << ", " << j << ")" << std::endl;
                    // mtx_cout.unlock();
                    std::thread p(simulatePlant,i,j);
                    p.detach();
                    mtx_total.lock();
                    total_entidades++;
                    mtx_total.unlock();
                }if (entity_grid[i][j].type == herbivore && entity_grid[i][j].age == 0) {
                    mtx_cout.lock();
                        std::cout << "Herbivore detected at position: (" << i << ", " << j << ")" << std::endl;
                        mtx_cout.unlock();
                        std::thread h(simulateHerbivore,i,j);
                        h.detach();
                        mtx_total.lock();
                    total_entidades++;
                    mtx_total.unlock();
                    }if (entity_grid[i][j].type == carnivore && entity_grid[i][j].age == 0) {
                        mtx_cout.lock();
                        std::cout << "Carnivore detected at position: (" << i << ", " << j << ")" << std::endl;
                        mtx_cout.unlock();
                        std::thread c(simulateCarnivore,i,j);
                        c.detach();
                                           mtx_total.lock();
                    total_entidades++;
                    mtx_total.unlock();
                    }
            }
        }  

    // Aguarda um curto período de tempo antes de notificar as threads de que a simulação da iteração terminou
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::unique_lock<std::mutex> lock2(mtx_wait_end);

        cv_thread.notify_all();  // Notifica todas as threads esperando na condição cv_thread

        // Aguarda até que todas as iterações terminem antes de continuar
        while (true) {
            cv_grid.wait(lock2);
            mtx_counter.lock();
            int it = *iteracao_terminada;
            std::cout << it << " total " << total_entidades << std::endl;
            mtx_counter.unlock();
            if (it >= total_entidades) {
                break;
            }
        }
      

       
        // Return the JSON representation of the entity grid
        nlohmann::json json_grid = entity_grid;
        return json_grid.dump(); });
    app.port(8080).run();

    return 0;
}