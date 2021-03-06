#include "ising.h"

std::ofstream ofile;


void output(int, int, double, double *);

int main(int argc, char* argv[])
{
    std::string filename;
    int dim_lattice, MC_cycles;
    double T_min, T_max, dT, T, average[5], total_average[5];
    bool random_lattice;
    int number_of_cores, rank_process;


    //  MPI initializations
    MPI_Init (&argc, &argv);
    MPI_Comm_size (MPI_COMM_WORLD, &number_of_cores);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank_process);

    if (rank_process == 0 && argc < 7){
      std::cout << "Bad Usage: " << argv[0] <<
        " Please provide lattice dimension, MC cycles, initial temperature, final temperature, tempurate step and output filename. " <<
        std::endl;
      exit(1);
    }

    if (argc == 7) {
      // Read in values from command line
      dim_lattice = atoi(argv[1]);
      MC_cycles   = atoi(argv[2]);
      T_min       = atof(argv[3]);
      T_max       = atof(argv[4]);
      dT          = atof(argv[5]);
      filename    = argv[6];
    }

    // Declare new file name and add lattice size to file name, only master node opens file
    if (rank_process == 0) {
      std::string folder = "../Data/";
      std::string fileout = folder + filename;
      ofile.open(fileout);
    }

    // Set up number of MC cycles for each core
    int no_intervalls = MC_cycles/number_of_cores;
    int myloop_begin = rank_process*no_intervalls + 1;
    int myloop_end = (rank_process+1)*no_intervalls;
    if ( (rank_process == number_of_cores-1) && (myloop_end < MC_cycles) ) myloop_end = MC_cycles;

    // broadcast to all nodes common variables since only master node reads from command line
    MPI_Bcast (&dim_lattice, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast (&T_min, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast (&T_max, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast (&dT, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);


    // Start timer
    double  TimeStart, TimeEnd, TotalTime;
    TimeStart = MPI_Wtime();


    double T_c = 2.269;
    Ising model(dim_lattice);
    for ( double temperature = T_min; temperature <= T_max; temperature+=dT){
          // Initialize lattice for low temperatures with all spins up
          if (temperature < T_c){
            random_lattice = false;
            model.InitializeLattice(temperature, random_lattice);
          }
          // Initialize lattice for higher temperatures with random spin directions
          else{
            random_lattice = true;
            model.InitializeLattice(temperature, random_lattice);
          }

          // Making sure average and total_average is set to 0 before calculations
          for( int i = 0; i < 5; i++) average[i] = 0.;
          for( int i = 0; i < 5; i++) total_average[i] = 0.;

          // Start Monte Carlo computation
          for (int cycles = myloop_begin; cycles <= myloop_end; cycles++)
          {
            // Run Metropolis
            model.Metropolis();

            // Save quantities
        		average[0] += model.energy;
        		average[1] += model.energy * model.energy;
        		average[2] += model.magnetization;
        		average[3] += model.magnetization * model.magnetization;
        		average[4] += fabs(model.magnetization);

          }

          // Find total average of quantites from all cores
          for( int i =0; i < 5; i++){
            MPI_Reduce(&average[i], &total_average[i], 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
          }
          // Print results and current temperature step
          if ( rank_process == 0) {
            output(dim_lattice, MC_cycles, temperature, total_average);
            std::cout << temperature << std::endl;
          }
      }

      ofile.close();  // close output file
      TimeEnd = MPI_Wtime();
      TotalTime = TimeEnd-TimeStart;
      if ( rank_process == 0) {
        // Print out final time and nmber of processors used
        std::cout << "Time = " <<  TotalTime  << " on number of processors: "  << number_of_cores  << std::endl;
      }

      // End MPI
      MPI_Finalize ();
      return 0;
    }

void output(int n_spins, int mcs, double temperature, double *total_average)
{
  using namespace std;

  double norm = 1/((double) (mcs));  // Divided by total number of cycles
  double E_normalized = total_average[0]*norm;
  double E2_normalized = total_average[1]*norm;
  double M_normalized = total_average[2]*norm;
  double M2_normalized = total_average[3]*norm;
  double Mabs_normalized = total_average[4]*norm;

  // All expectation values are per spin, divide by 1/n_spins/n_spins
  double E_variance = (E2_normalized - E_normalized*E_normalized) / (n_spins*n_spins);
  double susceptibility = (M2_normalized - Mabs_normalized*Mabs_normalized) / (n_spins*n_spins*temperature);

  double mean_energy = E_normalized / (n_spins*n_spins);
  double mean_magnetization = M_normalized / (n_spins*n_spins);
  double specific_heat = E_variance / (temperature*temperature);
  double mean_absolute_magnetization = Mabs_normalized / (n_spins*n_spins);

  ofile << setiosflags(ios::showpoint | ios::uppercase);
  ofile << setw(15) << setprecision(8) << temperature;
  ofile << setw(15) << setprecision(8) << mean_energy;
  ofile << setw(15) << setprecision(8) << specific_heat;
  ofile << setw(15) << setprecision(8) << mean_magnetization;
  ofile << setw(15) << setprecision(8) << susceptibility;
  ofile << setw(15) << setprecision(8) << mean_absolute_magnetization << endl;
} // end output function
