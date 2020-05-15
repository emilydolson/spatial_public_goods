#ifndef _PublicGoods_MODEL_H
#define _PublicGoods_MODEL_H

#include "ResourceGradient.h"
#include "config/ArgManager.h"
#include "Evolve/World.h"
#include "tools/spatial_stats.h"

EMP_BUILD_CONFIG( PublicGoodsConfig,
  GROUP(MAIN, "Global settings"),
  VALUE(SEED, int, -1, "Random number generator seed"),
  VALUE(TIME_STEPS, int, 1000, "Number of time steps to run for"),
  VALUE(WORLD_Y, size_t, 100, "Length of plate (in cells)"),
  VALUE(WORLD_X, size_t, 100, "Width of plate (in cells)"),
  VALUE(WORLD_Z, size_t, 1, "Depth of plate (in cells)"), 
  VALUE(INIT_POP_SIZE, int, 100, "Number of cells to seed population with"),
  VALUE(DATA_RESOLUTION, int, 10, "How many updates between printing data?"),

  GROUP(CELL, "Cell settings"),
  VALUE(MITOSIS_PROB, double, .5, "Probability of mitosis"),
  VALUE(AGE_LIMIT, int, 100, "Age over which non-stem cells die"),
  VALUE(RESISTANCE_MUT_STDEV, double, .01, "Standard deviation of gaussian controlling resistance mutation"),

  GROUP(PUBLIC_GOOD, "Public good settings"),
  VALUE(INITIAL_PUBLIC_GOOD_LEVEL, double, 0, "Initial quantity of public good (will be placed in all cells)"),
  VALUE(PUBLIC_GOOD_DIFFUSION_COEFFICIENT, double, .1, "Public good diffusion coefficient"),
  VALUE(DIFFUSION_STEPS_PER_TIME_STEP, int, 10, "Rate at which diffusion is calculated relative to rest of model"),
  // VALUE(PUBLIC_GOOD_THRESHOLD, double, .1, "How much public_good do cells need to survive?"),
  VALUE(KM, double, 0.01, "Michaelis-Menten kinetic parameter"),
);

struct Cell {
    int age = 0;
    bool producer = false;
    double resistance = 0;

    Cell(double in_resistance=0, bool in_producer=false) 
      : resistance(in_resistance), producer(in_producer) {;}

    bool operator== (const Cell & other) const {
      return age == other.age && producer == other.producer;
    }

    bool operator< (const Cell & other) const {
      return age*resistance < other.age*other.resistance;
    }

};

class HCAWorld : public emp::World<Cell> {
  protected:
  int TIME_STEPS;
  double PUBLIC_GOOD_DIFFUSION_COEFFICIENT;
  double MITOSIS_PROB;
  int AGE_LIMIT;
  int INIT_POP_SIZE;
  int DIFFUSION_STEPS_PER_TIME_STEP;
  double BASAL_PUBLIC_GOOD_CONSUMPTION = 0;
  double INITIAL_PUBLIC_GOOD_LEVEL;
  double KM;
  double RESISTANCE_MUT_STDEV;

  size_t WORLD_X;
  size_t WORLD_Y;
  size_t WORLD_Z;

  public:
  emp::Ptr<ResourceGradient> public_good;

  HCAWorld(emp::Random & r) : emp::World<Cell>(r), public_good(nullptr) {;}
  HCAWorld() {;}

  ~HCAWorld() {
    if (public_good) {
      public_good.Delete();
    }
  }

  void InitConfigs(PublicGoodsConfig & config) {
    TIME_STEPS = config.TIME_STEPS();
    MITOSIS_PROB = config.MITOSIS_PROB();
    PUBLIC_GOOD_DIFFUSION_COEFFICIENT = config.PUBLIC_GOOD_DIFFUSION_COEFFICIENT();

    AGE_LIMIT = config.AGE_LIMIT();
    INITIAL_PUBLIC_GOOD_LEVEL = config.INITIAL_PUBLIC_GOOD_LEVEL();
    DIFFUSION_STEPS_PER_TIME_STEP = config.DIFFUSION_STEPS_PER_TIME_STEP();
    KM = config.KM();
    INIT_POP_SIZE = config.INIT_POP_SIZE();
    RESISTANCE_MUT_STDEV = config.RESISTANCE_MUT_STDEV();

    WORLD_X = config.WORLD_X();
    WORLD_Y = config.WORLD_Y();
    WORLD_Z = config.WORLD_Z();

    if (public_good) {
      public_good->SetDiffusionCoefficient(PUBLIC_GOOD_DIFFUSION_COEFFICIENT);
    }
  }

  size_t GetWorldX() {
    return WORLD_X;
  }

  size_t GetWorldY() {
    return WORLD_Y;
  }

  size_t GetWorldZ() {
    return WORLD_Z;
  }


  void InitPop() {
    // for (int cell_id = 0; cell_id < WORLD_X * WORLD_Y; cell_id++) {
    //   InjectAt(Cell(CELL_STATE::HEALTHY), cell_id);
    // }
    pop.resize(WORLD_X*WORLD_Y);
    size_t initial_spot = random_ptr->GetUInt(WORLD_Y*WORLD_X);
    InjectAt(Cell(), initial_spot);

    for (size_t cell_id = 0; cell_id < (size_t)INIT_POP_SIZE; cell_id++) {
      size_t spot = random_ptr->GetUInt(WORLD_Y*WORLD_X);
      while (spot == initial_spot) {
        spot = random_ptr->GetUInt(WORLD_Y*WORLD_X);        
      }
      AddOrgAt(emp::NewPtr<Cell>(random_ptr->GetDouble(), random_ptr->P(.5)), spot, initial_spot);
    }

    RemoveOrgAt(initial_spot);
  }

  void InitPublicGood() {
    for (size_t x = 0; x < WORLD_X; x++) {
      for (size_t y = 0; y < WORLD_Y; y++) {
        for (size_t z = 0; z < WORLD_Z; z++) {
          public_good->SetVal(x, y, z, INITIAL_PUBLIC_GOOD_LEVEL);
        }
      }
    }
  }

  ResourceGradient& GetPublicGood() {
    return *public_good;
  }

  void UpdatePublicGood() {
      BasalPublicGoodConsumption();
      public_good->Diffuse();
      public_good->Update();

      // TODO: Producers produce public good
      for (size_t cell_id = 0; cell_id < GetSize(); cell_id++) {
        if (IsOccupied(cell_id) && GetOrg(cell_id).producer) {
          public_good->SetVal(cell_id % WORLD_X, cell_id / WORLD_X, 0, 1);
        }
      }

  }

  void Reset(PublicGoodsConfig & config, bool web = false) {
    Clear();
    if (public_good) {
      public_good.Delete();
      public_good = nullptr;
    }
    Setup(config, web);    
  }

  void Setup(PublicGoodsConfig & config, bool web = false) {
    InitConfigs(config);
    public_good.New(WORLD_X, WORLD_Y, WORLD_Z);
    public_good->SetDiffusionCoefficient(PUBLIC_GOOD_DIFFUSION_COEFFICIENT);

    if (!web) { // Web version needs to do diffusion separately to visualize
      OnUpdate([this](int ud){
        for (int i = 0; i < DIFFUSION_STEPS_PER_TIME_STEP; i++) {
          UpdatePublicGood();
        }
      });
    }


    // SetupFitnessFile().SetTimingRepeat(config.DATA_RESOLUTION());
    SetupPopulationFile().SetTimingRepeat(config.DATA_RESOLUTION());

    SetPopStruct_Grid(WORLD_X, WORLD_Y, true);
    InitPublicGood();
    InitPop();

    SetSynchronousSystematics(true);
  }

  void BasalPublicGoodConsumption() {
    for (size_t cell_id = 0; cell_id < pop.size(); cell_id++) {
      if (IsOccupied(cell_id)) {
        size_t x = cell_id % WORLD_X;
        size_t y = cell_id / WORLD_X;
        double public_good_loss_multiplier = public_good->GetVal(x, y, 0);
        public_good_loss_multiplier /= public_good_loss_multiplier + KM;
        public_good->DecNextVal(x, y, 0, BASAL_PUBLIC_GOOD_CONSUMPTION * public_good_loss_multiplier);
      }
    }
  }


  /// Determine if cell can divide (i.e. is space available). If yes, return
  /// id of cell that it can divide into. If not, return -1.
  int CanDivide(size_t cell_id) {
    emp::vector<int> open_spots;
    int x_coord = (int)(cell_id % WORLD_X);
    int y_coord = (int)(cell_id / WORLD_X);
    
    // Iterate over 9-cell neighborhood. Currently checks focal cell uneccesarily,
    // but that shouldn't cause problems because it will never show up as invasible.
    for (int x = std::max(0, x_coord-1); x < std::min((int)WORLD_X, x_coord + 2); x++) {
      for (int y = std::max(0, y_coord-1); y < std::min((int)WORLD_Y, y_coord + 2); y++) {
        int this_cell = y*(int)WORLD_X + x;
        // Cells can be divided into if they are empty or if they are healthy and the
        // dividing cell is cancerous
        if (!IsOccupied((size_t)this_cell)) {
          open_spots.push_back(this_cell);
        }
      }
    }
    
    // -1 is a sentinel value indicating no spots are available
    if (open_spots.size() == 0) {
      return -1;
    }

    // If there are one or more available spaces, return a random spot
    return open_spots[random_ptr->GetUInt(0, open_spots.size())];
  }

  int Mutate(emp::Ptr<Cell> c){
    c->age = 0;
    c->resistance += random_ptr->GetRandNormal(0, RESISTANCE_MUT_STDEV);
    return 0;
  }


  void Quiesce(size_t cell_id) {
    // Quiescence - stick the cell back into the population in
    // the same spot but don't change anything else
    // std::cout << "Quieseing" << std::endl;
    pop[cell_id]->age++;
    if (pop[cell_id]->age < AGE_LIMIT) {
      emp::Ptr<Cell> cell = emp::NewPtr<Cell>(*pop[cell_id]);
      AddOrgAt(cell, emp::WorldPosition(cell_id,1), cell_id);      
    }
  }

  void RunStep() {
    std::cout << update << std::endl;

    for (size_t cell_id = 0; cell_id < WORLD_X * WORLD_Y; cell_id++) {
      if (!IsOccupied(cell_id)) {
        // Don't need to do anything for dead/empty cells
        continue;
      }

      size_t x = cell_id % WORLD_X;
      size_t y = cell_id / WORLD_X;

      // Query public_good to test for sufficient quantity
      // if (public_good->GetVal(x, y, 0) < PUBLIC_GOOD_THRESHOLD) {
      //   // If hypoxic, the hif1-alpha surpressor gets turned off
      //   // causing hif1-alpha to accumulate
      //   pop[cell_id]->hif1alpha = 1;

      //   // If hypoxic, die with specified probability
      //   if (!random_ptr->P(HYPOXIA_DEATH_PROB)) {
      //     Quiesce(cell_id); // Cell survives to next generation
      //   }
      //   continue; // Division not allowed under hypoxia 
      // }

      // Check for space for division
      int potential_offspring_cell = CanDivide(cell_id);

      // If space, divide
      if (potential_offspring_cell != -1 && random_ptr->P(MITOSIS_PROB)) {
        

        // Handle daughter cell in previously empty spot
        before_repro_sig.Trigger(cell_id);
        emp::Ptr<Cell> offspring = emp::NewPtr<Cell>(*pop[cell_id]);
        Mutate(offspring);
        offspring_ready_sig.Trigger(*offspring, cell_id);
        AddOrgAt(offspring, emp::WorldPosition((size_t)potential_offspring_cell, 1), cell_id);

        // Handle daughter cell in current location
        before_repro_sig.Trigger(cell_id);
        offspring = emp::NewPtr<Cell>(*pop[cell_id]);
        Mutate(offspring);
        offspring_ready_sig.Trigger(*offspring, cell_id);
        AddOrgAt(offspring, emp::WorldPosition(cell_id,1), cell_id);
        // std::cout << "Mutated: " << offspring->clade << std::endl;
      } else {        
        Quiesce(cell_id);
      }
    }

    Update();
  }

  void Run() {
      for (int u = 0; u <= TIME_STEPS; u++) {
          RunStep();
      }
  }

};

#endif