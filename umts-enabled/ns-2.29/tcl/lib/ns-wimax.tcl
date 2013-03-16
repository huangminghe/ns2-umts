# This class contains default value for tcl

Phy/WirelessPhy/OFDM set g_ 0

Mac/802_16 set queue_length_         50 ;#maximum number of packets

Mac/802_16 set frame_duration_       0.004
Mac/802_16 set channel_              0
Mac/802_16 set fbandwidth_           5e+6
Mac/802_16 set rtg_                  10
Mac/802_16 set ttg_                  10
Mac/802_16 set dcd_interval_         5 ;#max 10s
Mac/802_16 set ucd_interval_         5 ;#max 10s
Mac/802_16 set init_rng_interval_    1 ;#max 2s
Mac/802_16 set lost_dlmap_interval_  0.6
Mac/802_16 set lost_ulmap_interval_  0.6
Mac/802_16 set t1_timeout_           [expr 5* [Mac/802_16 set dcd_interval_]]
Mac/802_16 set t2_timeout_           [expr 5* [Mac/802_16 set init_rng_interval_]]
Mac/802_16 set t3_timeout_           0.2
Mac/802_16 set t6_timeout_           3
Mac/802_16 set t12_timeout_          [expr 5* [Mac/802_16 set ucd_interval_]]
Mac/802_16 set t16_timeout_          3 ;#qos dependant
Mac/802_16 set t17_timeout_          5 
Mac/802_16 set t21_timeout_          0.02 ;#max 10s. Use 20ms to replace preamble scanning
Mac/802_16 set contention_rng_retry_ 16 
Mac/802_16 set invited_rng_retry_    16 ;#16
Mac/802_16 set request_retry_        2 ;#16
Mac/802_16 set reg_req_retry_        3
Mac/802_16 set tproc_                0.001
Mac/802_16 set dsx_req_retry_        3 
Mac/802_16 set dsx_rsp_retry_        3

Mac/802_16 set rng_backoff_start_    2
Mac/802_16 set rng_backoff_stop_     6
Mac/802_16 set bw_backoff_start_     2
Mac/802_16 set bw_backoff_stop_      6

Mac/802_16 set scan_duration_        50
Mac/802_16 set interleaving_interval_ 50
Mac/802_16 set scan_iteration_       2
Mac/802_16 set t44_timeout_          0.1
Mac/802_16 set max_dir_scan_time_    0.2 ;#max scan for each neighbor BSs
Mac/802_16 set client_timeout_       0.5 
Mac/802_16 set nbr_adv_interval_     0.5
Mac/802_16 set scan_req_retry_       3

Mac/802_16 set lgd_factor_           1
Mac/802_16 set rxp_avg_alpha_        1
Mac/802_16 set delay_avg_alpha_      1
Mac/802_16 set jitter_avg_alpha_     1
Mac/802_16 set loss_avg_alpha_       1
Mac/802_16 set throughput_avg_alpha_ 1
Mac/802_16 set throughput_delay_     0.02
Mac/802_16 set print_stats_          0

Agent/WimaxCtrl set debug_ 0
Agent/WimaxCtrl set adv_interval_ 1.0
Agent/WimaxCtrl set default_association_level_ 0
Agent/WimaxCtrl set synch_frame_delay_ 50

