import ipaddress 

from lib.experiment_codebase import *

class MortyCodebase(ExperimentCodebase):

    def get_client_cmd(self, config, i, j, k, run, local_exp_directory,
            remote_exp_directory):
        name, _ = os.path.splitext(config['network_config_file_name'])
        closest_replica = i // config['num_groups'] if config['server_emulate_wan'] else -1
        if 'run_locally' in config and config['run_locally']:
            path_to_client_bin = os.path.join(config['src_directory'],
                    config['bin_directory_name'], config['client_bin_name'])
            exp_directory = local_exp_directory
            config_path = os.path.join(local_exp_directory, config['network_config_file_name'])
            stats_file = os.path.join(exp_directory,
                    config['out_directory_name'], 'client-%d-%d' % (i, j),
                    'client-%d-%d-%d-stats-%d.json' % (i, j, k, run))
        else:
            path_to_client_bin = os.path.join(
                    config['base_remote_bin_directory_nfs'],
                    config['bin_directory_name'], config['client_bin_name'])
            exp_directory = remote_exp_directory
            config_path = os.path.join(remote_exp_directory, config['network_config_file_name'])
            stats_file = os.path.join(exp_directory,
                    config['out_directory_name'],
                    'client-%d-%d-%d-stats-%d.json' % (i, j, k, run))

        
        client_threads = 1 if not 'client_threads_per_process' in config else config['client_threads_per_process']

        client_id = i * config['client_nodes_per_server'] * config['client_processes_per_client_node'] + j * config['client_processes_per_client_node'] + k
        client_command = ' '.join([str(x) for x in [
            path_to_client_bin,
            '--client_id', client_id,
            '--benchmark', config['benchmark_name'],
            '--exp_duration', config['client_experiment_length'],
            '--warmup_secs', config['client_ramp_up'],
            '--config_path', config_path,
            '--num_shards', config['num_shards'],
            '--num_groups', config['num_groups'],
            '--protocol_mode', config['client_protocol_mode'],
            '--closest_replica', closest_replica,
            '--stats_file', stats_file,
            '--num_clients', client_threads]])

        if config['replication_protocol'] == 'tapir':
            if 'sync_commit' in config['replication_protocol_settings']:
                client_command += ' --tapir_sync_commit=%s' % (str(config['replication_protocol_settings']['sync_commit']).lower())

        if 'message_transport_type' in config['replication_protocol_settings']:
            client_command += ' --trans_protocol %s' % config['replication_protocol_settings']['message_transport_type']

        if 'client_abort_backoff' in config:
            client_command += ' --abort_backoff=%d' % config['client_abort_backoff']
        if 'client_retry_aborted' in config:
            client_command += ' --retry_aborted=%s' % (str(config['client_retry_aborted']).lower())
        if 'client_max_attempts' in config:
            client_command += ' --max_attempts %d' % config['client_max_attempts']

        if config['benchmark_name'] == 'retwis':
            client_command += ' --num_keys %d' % config['client_num_keys']
            if 'client_key_selector' in config:
                client_command += ' --key_selector %s' % config['client_key_selector']
                if config['client_key_selector'] == 'zipf':
                    client_command += ' --zipf_coefficient %f' % config['client_zipf_coefficient']
        elif config['benchmark_name'] == 'rw':
            client_command += ' --num_keys %d' % config['client_num_keys']
            client_command += ' --num_ops_txn %d' % config['rw_num_ops_txn']
            if 'client_key_selector' in config:
                client_command += ' --key_selector %s' % config['client_key_selector']
                if config['client_key_selector'] == 'zipf':
                    client_command += ' --zipf_coefficient %f' % config['client_zipf_coefficient']
        elif config['benchmark_name'] == 'tpcc':
            client_command += ' --tpcc_num_warehouses %d' % config['tpcc_num_warehouses']
            client_command += ' --tpcc_w_id %d' % (client_id % config['tpcc_num_warehouses'] + 1)
            client_command += ' --tpcc_C_c_id %d' % config['tpcc_c_c_id']
            client_command += ' --tpcc_C_c_last %d' % config['tpcc_c_c_last']
            client_command += ' --tpcc_stock_level_ratio %d' % config['tpcc_stock_level_ratio']
            client_command += ' --tpcc_delivery_ratio %d' % config['tpcc_delivery_ratio']
            client_command += ' --tpcc_order_status_ratio %d' % config['tpcc_order_status_ratio']
            client_command += ' --tpcc_payment_ratio %d' % config['tpcc_payment_ratio']
            client_command += ' --tpcc_new_order_ratio %d' % config['tpcc_new_order_ratio']
        elif config['benchmark_name'] == 'smallbank':
            client_command += ' --balance_ratio %d' % config['smallbank_balance_ratio']
            client_command += ' --deposit_checking_ratio %d' % config['smallbank_deposit_checking_ratio']
            client_command += ' --transact_saving_ratio %d' % config['smallbank_transact_saving_ratio']
            client_command += ' --amalgamate_ratio %d' % config['smallbank_amalgamate_ratio']
            client_command += ' --write_check_ratio %d' % config['smallbank_write_check_ratio']
            client_command += ' --num_hotspots %d' % config['smallbank_num_hotspots']
            client_command += ' --num_customers %d' % config['smallbank_num_customers']
            client_command += ' --hotspot_probability %f' % config['smallbank_hotspot_probability']
            client_command += ' --customer_name_file_path %s' % config['smallbank_customer_name_file_path']

        if 'run_locally' in config and config['run_locally']:
            stdout_file = os.path.join(exp_directory,
                    config['out_directory_name'],
                    'client-%d-%d' % (i, j),
                    'client-%d-%d-%d-stdout-%d.log' % (i, j, k, run))
            stderr_file = os.path.join(exp_directory,
                    config['out_directory_name'],
                    'client-%d-%d' % (i, j),
                    'client-%d-%d-%d-stderr-%d.log' % (i, j, k, run))

            client_command = '%s 1> %s 2> %s' % (client_command, stdout_file,
                    stderr_file)
        else:
            stdout_file = os.path.join(exp_directory,
                    config['out_directory_name'],
                    'client-%d-%d-%d-stdout-%d.log' % (i, j, k, run))
            stderr_file = os.path.join(exp_directory,
                    config['out_directory_name'],
                    'client-%d-%d-%d-stderr-%d.log' % (i, j, k, run))
            if 'default_remote_shell' in config and config['default_remote_shell'] == 'bash':
                client_command = '%s 1> %s 2> %s' % (client_command, stdout_file,
                    stderr_file)
            else: 
                client_command = tcsh_redirect_output_to_files(client_command,
                    stdout_file, stderr_file)
        
        if 'pin_client_processes' in config and isinstance(config['pin_client_processes'], list) and len(config['pin_client_processes']) > 0:
            core = config['pin_client_processes'][k % len(config['pin_client_processes'])]
            client_command = 'taskset 0x%x %s' % (1 << core, client_command)
        
        if isinstance(config['client_debug_output'], str) or config['client_debug_output']:
            if 'run_locally' in config and config['run_locally'] or 'default_remote_shell' in config and config['default_remote_shell'] == 'bash':
                if isinstance(config['client_debug_output'], str):
                    client_command = 'DEBUG=%s %s' % (config['client_debug_output'], client_command)
                else:
                    client_command = 'DEBUG=all %s' % client_command
            else:
                if isinstance(config['client_debug_output'], str):
                    client_command = 'setenv DEBUG %s; %s' % (config['client_debug_output'], client_command)
                else:
                    client_command = 'setenv DEBUG all; %s' % client_command
        client_command = '(cd %s; %s) & ' % (exp_directory, client_command)
        return client_command

    def get_replica_cmd(self, config, replica_id, process_id, group, run, local_exp_directory,
            remote_exp_directory):
        group = replica_id % config['num_groups']
        name, ext = os.path.splitext(config['network_config_file_name'])
        if  'run_locally' in config and config['run_locally']:
            path_to_server_bin = os.path.join(config['src_directory'],
                    config['bin_directory_name'], config['server_bin_name'])
            exp_directory = local_exp_directory
            config_file = os.path.join(local_exp_directory, config['network_config_file_name'])
            stats_file = os.path.join(exp_directory,
                    config['out_directory_name'], 'server-%d' % replica_id,
                    'server-%d-stats-%d.json' % (replica_id, run))
        else:
            path_to_server_bin = os.path.join(
                    config['base_remote_bin_directory_nfs'],
                    config['bin_directory_name'], config['server_bin_name'])
            exp_directory = remote_exp_directory
            config_file = os.path.join(remote_exp_directory, config['network_config_file_name'])
            stats_file = os.path.join(exp_directory,
                    config['out_directory_name'],
                    'server-%d-stats-%d.json' % (replica_id, run))


        replica_command = ' '.join([str(x) for x in [
            path_to_server_bin,
            '--config_path', config_file,
            '--replica_idx', replica_id // config['num_groups'],
            '--protocol', config['replication_protocol'],
            '--num_shards', config['num_shards'],
            '--num_groups', config['num_groups'],
            '--stats_file', stats_file,
            '--group_idx', group]])

        if 'message_transport_type' in config['replication_protocol_settings']:
            replica_command += ' --trans_protocol %s' % config['replication_protocol_settings']['message_transport_type']

        if 'server_debug_stats' in config and config['server_debug_stats']:
            replica_command += ' --debug_stats'

        if config['benchmark_name'] == 'retwis':
            replica_command += ' --num_keys %d' % config['client_num_keys']
        elif config['benchmark_name'] == 'rw':
            replica_command += ' --num_keys %d' % config['client_num_keys']
        elif config['benchmark_name'] == 'tpcc':
            replica_command += ' --data_file_path %s' % config['tpcc_data_file_path']
            replica_command += ' --partitioner warehouse'
        elif config['benchmark_name'] == 'smallbank':
            replica_command += ' --data_file_path %s' % config['smallbank_data_file_path']


        if 'run_locally' in config and config['run_locally']:
            stdout_file = os.path.join(exp_directory,
                    config['out_directory_name'], 'server-%d' % replica_id,
                    'server-%d-stdout-%d.log' % (replica_id, run))
            stderr_file = os.path.join(exp_directory,
                    config['out_directory_name'], 'server-%d' % replica_id,
                    'server-%d-stderr-%d.log' % (replica_id, run))
            replica_command = '%s 1> %s 2> %s' % (replica_command, stdout_file,
                    stderr_file)
        else:
            stdout_file = os.path.join(exp_directory,
                    config['out_directory_name'], 'server-%d-stdout-%d.log' % (
                        replica_id, run))
            stderr_file = os.path.join(exp_directory,
                    config['out_directory_name'], 'server-%d-stderr-%d.log' % (
                        replica_id, run))
            if 'default_remote_shell' in config and config['default_remote_shell'] == 'bash':
                replica_command = '%s 1> %s 2> %s' % (replica_command, stdout_file,
                    stderr_file)
            else:
                replica_command = tcsh_redirect_output_to_files(replica_command,
                    stdout_file, stderr_file)
        
        if 'pin_server_process' in config and config['pin_server_process'] >= 0:
            core = 0
            replica_command = 'taskset 0x%x %s' % (1 << core, replica_command)

        if isinstance(config['server_debug_output'], str) or config['server_debug_output']:
            if 'run_locally' in config and config['run_locally'] or 'default_remote_shell' in config and config['default_remote_shell'] == 'bash':
                if isinstance(config['server_debug_output'], str):
                    replica_command = 'DEBUG=%s %s' % (config['server_debug_output'], replica_command)
                else:
                    replica_command = 'DEBUG=all %s' % replica_command
            else:
                if isinstance(config['server_debug_output'], str):
                    replica_command = 'setenv DEBUG %s; %s' % (config['server_debug_output'], replica_command)
                else:
                    replica_command = 'setenv DEBUG all; %s' % replica_command
        replica_command = 'cd %s; %s' % (exp_directory, replica_command)
        return replica_command

    def prepare_local_exp_directory(self, config, config_file):
        local_exp_directory = super().prepare_local_exp_directory(config, config_file)
        config_file = os.path.join(local_exp_directory, config['network_config_file_name'])
        with open(config_file, 'w') as f:
            for group in range(config['num_groups']):
                n = len(config['server_names']) // config['num_groups']
                print('f %d' % ((n - 1) // 2), file=f)
                print('group', file=f)
                if 'run_locally' in config and config['run_locally']:
                    for i in range(n):
                        print('replica %s:%d' % ('localhost',
                            config['server_port'] + i + config['num_groups'] * group), file=f)
                else:
                    for i in range(n):
                        print('replica %s:%d' % (config['server_names'][i * config['num_groups'] + group],
                            config['server_port']), file=f)

        return local_exp_directory
    
    def prepare_remote_server_codebase(self, config, host, local_exp_directory, remote_out_directory):
        pass

    def setup_nodes(self, config):
        pass

