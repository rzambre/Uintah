# Uintah that uses Hypre's MPI EndPoint version

## Hypre using MPI End Points:
This is the modified Uintah that uses Hypre's MPI EndPoint version. More details are presented in "Sahasrabudhe D., Berzins M. (2020) Improving Performance of the Hypre Iterative Solver for Uintah Combustion Codes on Manycore Architectures Using MPI Endpoints and Kernel Consolidation. In: Krzhizhanovskaya V. et al. (eds) Computational Science – ICCS 2020. ICCS 2020. Lecture Notes in Computer Science, vol 12137. Springer, Cham. https://doi.org/10.1007/978-3-030-50371-0_13"  
Hypre's EndPoint version is present at https://github.com/damu1000/hypre_ep

## Installation  

git clone https://github.com/damu1000/Uintah.git uintah_src  
mkdir 1_mpi  
mkdir 2_ep  
mkdir work_dir

### Install MPI Only version  
cd 1_mpi  
#configure line is for intel compilers and KNL. --enable-kokkos will automatically download and install kokkos.  
../uintah_src/src/configure --enable-64bit --enable-optimize="-std=c++11 -O2 -g -fopenmp -mkl -fp-model precise -xMIC-AVX512" --enable-assertion-level=0 --with-mpi=built-in --with-hypre=<hypre_path>/hypre_cpu/src/build/ LDFLAGS="-ldl" --enable-examples --enable-arches --enable-kokkos  
make -j16 sus  
cp ./StandAlone/sus ../work_dir/1_mpi  

### Install EP version  
cd ../2_ep  
#Note the difference between mpi only line and ep line: --with-hypre option is changed from hypre_cpu to hypre_ep and CXXFLAGS is added.  
../uintah_src/src/configure --enable-64bit --enable-optimize="-std=c++11 -O2 -g -fopenmp -mkl -fp-model precise -xMIC-AVX512" --enable-assertion-level=0 --with-mpi=built-in --with-hypre=<hypre_path>/hypre_ep/src/build/ LDFLAGS="-ldl" CXXFLAGS="-I<hypre_path>/hypre_ep/src/ -DUSE_MPI_EP" --enable-examples --enable-arches --enable-kokkos  
make -j16 sus  
cp ./StandAlone/sus ../work_dir/2_ep

### Run

#There are two examples - solvertest1.ups and RMCRT_bm1_DO_solvertest.ups: solvertest1 is a lapace equation built within Uintah and solved using Hypre. RMCRT_bm1_DO_solvertest is a dummy combination of the RMCRT task within Uintah followed by solvertest1.  
cd ../work_dir  
#copy both input spec files:  
cp ../uintah_src/src/StandAlone/inputs/Examples/solvertest1.ups ./  
cp ../uintah_src/src/StandAlone/inputs/Examples/RMCRT_bm1_DO_solvertest.ups ./  

#modify both input files as follows:  
#1. Ensure ups file has: &lt;outputTimestepInterval&gt;0&lt;/outputTimestepInterval&gt; and &lt;checkpoint cycle = "0" interval = "0"/&gt;. This will turn off the exporting of the output to the disk. Keep the output on if changes made within Uintah or Hypre are to be tested.  
#2. Update &lt;resolution&gt; and &lt;patches&gt; as needed. &lt;resolution&gt; is the total number of cells within the domain and &lt;patches&gt; gives the nuber patches in three dimensions. e.g. if &lt;resolution&gt; is set to [128,128,128] and &lt;patches&gt; is set to [4,4,4], then there will 64 patches in total (4 in each dimension) and each patch will have the size [128/4,128/4,128/4] i.e. 32 cubed.  
#3. RMCRT_bm1_DO_solvertest.ups has two levels of the mesh. Each level has its own resolution settings. The thumb rule is to reduce fine level (level 1) resolution by a factor of 4 while setting coarse level (level 0) resolution.  
#4. Adjust max_Timesteps and maxiterations to the required value.

#The following examples are to occupy 64 cores on a KNL node:  
export OMP_NESTED=true
export OMP_PROC_BIND=spread,spread
export OMP_PLACES=threads

#run the MPI only version. Ensure there are at least 64 patches.
#Always set npartitions=1 and nthreadsperpartition=1 for mpi only version:  
export OMP_NUM_THREADS=1  
mpirun -np 64 ./1_mpi -npartitions 1 -nthreadsperpartition 1 &lt;input filename&gt;.ups  
#the above command will create 64 mpi ranks with 1 thread per rank.

#run the MPI EP version. Ensure there are at least 64 patches:  
# Ensure that the following block is present in the ups file.
	<LoadBalancer type="HypreEPLoadBalancer">
		<timestepInterval>100</timestepInterval>
	</LoadBalancer>


#xthreads x ythreads x zthreads x teamsize MUST match OMP_NUM_THREADS  
export OMP_NUM_THREADS=16  
mpirun -np 4 ./2_ep -xthreads 2 -xthreads 2 -xthreads 2 -teamsize 2 &lt;input filename&gt;.ups  
#the above command will create 4 mpi ranks with 2x2x2=8 teams per rank and 2 worker threads per team.


