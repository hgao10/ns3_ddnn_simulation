# Packet-level simulations

Packet-level simulation in ns-3 is often found to be challenging. This repository is aimed to get you to understand it more quickly, and to get you started. In bullet-points:

* **Basic simulation** -- This repository provides the functionality to (a) read in topologies, (b) do efficient ECMP switch-seeded 5-tuple routing, (c) do some heuristic optimization of TCP parameters, (d) do some heuristic optimization of qdiscs, and (e) show progress of the simulation as it is running. This is referred to as a basic simulation and whose source code can be found in `simulator/src/basic-sim`.

* **Run folder** -- A basic simulation is centered around a run folder (see below for details), which should provide all the input for a basic simulation run. A run folder must always have a `config_ns3.properties` and `topology.properties` file.

* It also already provides two applications: 

  **flows:** runs a schedule of basic flows (i.e., "from A to B transfer X bytes at time T").
  
  **pingmesh:** for each pair of endpoints, send a ping every X seconds to measure the RTT.
  
  ... which which are presented as "executables" in `simulator/scratch/main_{flows/pingmesh}` and whose source code can be found in `simulator/src/basic-apps`.

**Make sure you fork this repository and continue on it in your own repository. This repository is kept simple on purpose. You can keep your fork up-to-date by pulling it in regularly from your own repository: `git pull git@gitlab.inf.ethz.ch:OU-SINGLA/ns3-basic-sim.git`**

## Getting started

1. Extract ns-3.30.1.zip (this is given for convenience, this is the generic ns-3):
   ```
   unzip ns-3.30.1.zip
   ```
   
2. Now remove the scratch folder from the ns-3.30.1 folder (where it was extracted to), as we have our own:
   ```
   rm -rf ns-3.30.1/scratch
   ```
   
3. Move the contents to the simulator directory:
   ```
   cp -r ns-3.30.1/* simulator/
   ```
   
4. Remove the ns-3.30.1 folder as we do not need it anymore:
   ```
   rm -r ns-3.30.1
   ```

5. Navigate to simulator:
   ```
   cd simulator
   ```

6. Make sure you have a recent version of Python (3.6+)
   
7. Configure
    ```
    ./waf configure --enable-gcov --enable-examples --enable-tests --out=build/debug_all
    ```
   
8. Build
    ```
    ./waf
    ```
   
9. Run the example:
    ```
    ./waf --run="main_flows --run_dir='../runs/flows_example_single'"
    ```

10. Check out the results:
    ```
    xdg-open ../runs/flows_example_single/logs_ns3/flows.txt
    ```
 
 ## Running tests
 
To run all the tests for the basic simulation:

```
cd simulator
python test.py -v -s "basic-sim" -t test-results-basic-sim
python test.py -v -s "basic-apps" -t test-results-basic-apps
xdg-open test-results-basic-sim.txt
xdg-open test-results-basic-apps.txt
```
 
 ## Speed-up the simulation
 
If you want to speed-up the simulation, configuring as follows helps (this effectively removes all ns-3 logging statements in the code at compile-time):
 ```
 ./waf configure --build-profile=optimized --out=build/optimized
 ```

## Run folder

The run folder must contain the input of a simulation. It is the basis of every basic simulation.

**config_ns3.properties**

General properties of the simulation. The following MUST always be defined:

* `filename_topology` : Topology filename (relative to run folder)
* `simulation_end_time_ns` : How long to run the simulation in simulation time (ns)
* `simulation_seed` : If there is randomness present in the simulation, this guarantees reproducibility (exactly the same outcome) if the seed is the same
* `link_data_rate_megabit_per_s` : Data rate set for all links (Mbit/s)
* `link_delay_ns` : Propagation delay set for all links (ns)
* `link_max_queue_size_pkts` : Maximum queue size for all links (no. of packets)
* `disable_qdisc_endpoint_tors_xor_servers` : Whether to disable the traffic control queueing discipline at the endpoint nodes (if there are servers, servers, else those are the ToRs) (boolean: true/false)
* `disable_qdisc_non_endpoint_switches` : Whether to disable the traffic control queueing discipline at non-endpoint nodes (if there are servers, all switches incl. ToRs, else all switches excl. ToRs) (boolean: true/false)

**topology.properties**

The topological layout of the network. Please see the examples to understand each property. Besides it just defining a graph, the following rules apply:

* If there are servers defined, they can only have edges to a ToR.
* There is only a semantic difference between switches, switches which are ToRs and servers. If there are servers, only servers should be valid endpoints for applications. If there are no servers, ToRs should be valid endpoints instead.

**The log files**

There are is always one log file guaranteed generated by the run in the `logs_ns3` folder within the run folder:

* `finished.txt` : Contains "Yes" if the run has finished, "No" if not.

## Example application #1: flow schedule (scratch/main_flows)

The flow schedule is a very simple type of application. It schedules flows to start from A to B at time T to transfer X amount of bytes. It saves the results of the flow completion into useful file formats.

You MUST set the following key in `config_ns3.properties`:

* `filename_schedule` : Schedule filename (relative to run folder) (path/to/schedule.csv)

The following are OPTIONAL in `config_ns3.properties`:

* `enable_flow_logging_to_file_for_flow_ids` : Set of flow identifiers for which you want logging to file for progress, cwnd and RTT (located at `logs_dir/flow-[id]-{progress, cwnd, rtt}.txt`). Example value: `set(0, 1`) to log for flows 0 and 1. The file format is: `flow_id,now_in_ns,[progress_byte/cwnd_byte/rtt_ns])`.

**schedule.csv**

Flow arrival schedule. 

Each line defines a flow as follows:

```
flow_id,from_node_id,to_node_id,size_byte,start_time_ns,additional_parameters,metadata
```

Notes: flow_id must increment each line. All values except additional_parameters and metadata are mandatory. `additional_parameters` should be set if you want to configure something special for each flow in main.cc (e.g., different transport protocol). `metadata` you can use for identification later on in the flows.csv/txt logs (e.g., to indicate the workload or coflow it was part of).

**The flow log files**

There are two log files generated by the run in the `logs_ns3` folder within the run folder:

* `flows.txt` : Flow results in a visually appealing table.
* `flows.csv` : Flow results in CSV format for processing with each line:

   ```
   flow_id,from_node_id,to_node_id,size_byte,start_time_ns,end_time_ns,duration_ns,amount_sent_byte,[finished: YES/CONN_FAIL/NO_BAD_CLOSE/NO_ERR_CLOSE/NO_ONGOING],metadata
   ```

   (with `YES` = all data was sent and acknowledged fully and there was a normal socket close, `NO_CONN_FAIL` = connection failed (happens only in a very rare set of nearly impossible-to-reach state, typically `NO_BAD_CLOSE` is the outcome when something funky went down with the 3-way handshake), `NO_BAD_CLOSE` = socket closed normally but not all data was transferred (e.g., due to connection timeout), `NO_ERR_CLOSE` = socket error closed, `NO_ONGOING` = socket is still sending/receiving and is not yet closed)
   
**Flows example run**
```
cd simulator
./waf --run="main_flows --run_dir='../runs/flows_example_single'"
```

## Example application #2: pingmesh (scratch/main_pingmesh)

The pingmesh application is when you want to continuously sends UDP pings between endpoints to measure their RTT. 

You MUST set the following key in `config_ns3.properties`:

* `pingmesh_interval_ns` : Interval to send a ping (ns)

The following are OPTIONAL:

* `pingmesh_endpoint_pairs` : Endpoint directed pingmesh pairs (either `all` (default) or e.g., `set(0-1, 5-6)` to only have pinging from 0 to 1 and from 5 to 6 (directed pairs))

**The pingmesh log files**

There are two log files generated by the run in the `logs_ns3` folder within the run folder:

* `pingmesh.txt` : Pingmesh results in a visually appealing table.
* `pingmesh.csv` : Pingmesh results in CSV format for processing with each line:

   ```
   from_node_id,to_node_id,i,send_request_timestamp,reply_timestamp,receive_reply_timestamp,latency_to_there_ns,latency_from_there_ns,rtt_ns,[YES/LOST]
   ```
  
  (with `YES` = ping completed successfully, `LOST` = ping reply did not arrive (either it got lost, or the simulation ended before it could arrive))

**Pingmesh example run**
```
cd simulator
./waf --run="main_pingmesh --run_dir='../runs/pingmesh_example_single'"
```

## Example application #3: your own

To make your own application (let's call it "yourown"), first do the following:

1. Take a look at `simulator/src/basic-apps/model/flow-scheduler.cc/h` and `simulator/src/basic-apps/model/pingmesh-scheduler.cc/h` to see how to create something that makes use of basic-sim.
2. Take a look at `simulator/src/basic-sim/model/basic-simulation.cc/h` to know what it does (it is no different from one of the ns-3 tutorials except that it does the topology generation for you and has some nice stuff)
3. Take a look at `simulator/scratch/main_flows/main_flows.cc` and `simulator/scratch/main_pingmesh/main_pingmesh.cc`

Now we can get started:

1. Create a `simulator/basic-apps/model/yourown-scheduler.cc` and `simulator/basic-apps/model/yourown-scheduler.h`, maybe just make a copy of `pingmesh-scheduler.cc/h` to get you started on the structure.

2. Add `yourown-scheduler.cc/h` to `simulator/basic-apps/wscript` in the source and header section so it gets compiled

3. Create a `scratch/main_yourown/main_yourown.cc`, maybe just make a copy of `scratch/main_pingmesh` to get you started on the structure. 

4. Now you have to make your own application etc. etc. -- you can look at `src/applications/model` for some other example applications.

5. You could even consider splitting your work into your own ns-3 module (e.g., use `python create-module.py yourown` within `simulator/src` -- be sure to set `basic-sim` (and `basic-apps` if you use any of it) as your dependency)

## Some recommendations and notes

Based on some experience with packet-level simulation, I would like to add the following notes:

* **Strong recommendation: keep any run input generation (e.g., config/schedule/topology) separate from the main.cc!** Make a different repository that generates run folders, which is then inputted using main.cc. Mixing up the run folders and the main function will lead to unmaintainable code, and is also unnecessary: the simulator is for simulating, not for generating simulation input. It also makes it more difficult to reproduce old runs. Just put all the generation code into another project repository, in which you use languages like Python which are more effective at this job anyway.

* **Why is my simulation slow?** Discrete packet-level simulation speed (as in wallclock seconds for each simulation second) is chiefly determined by events of packets going being set over links. You can interpret this as follows:
  - The wallclock time it takes to simulate 10 flows going over a single link is about the same as 1 flow going over a link.
  - The wallclock time it takes to simulate a flow going over 5 links (hops) is roughly 5x as slow as a flow going over 1 link (hop).
  - The wallclock time it takes to simulate a flow going over a 10 Mbit/s link for 100s takes about as long as a flow going over a 100 Mbit/s link for 10s.
  - Idle links don't increase wallclock time because there are barely any events happening there (maybe routing updates).

* **To maintain reproducibility, any randomness inside your code must be drawn from the ns-3 randomness classes which were initialized by the simulation seed!** Runs must be reproducible in a discrete event simulation run.


## Acknowledgements

Based on code written by Hussain, who did his master thesis in the NDAL group.
Refactored, extended and maintained by Simon. The ECMP routing hashing function is inspired by https://github.com/mkheirkhah/ecmp (retrieved February 20th, 2020).
