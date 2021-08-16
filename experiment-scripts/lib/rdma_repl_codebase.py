'''
 Copyright 2021 Matthew Burke <matthelb@cs.cornell.edu>
                Florian Suri-Payer <fs435@cornell.edu>

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use, copy,
 modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

'''
import ipaddress 

from lib.experiment_codebase import *

class RdmaReplCodebase(ExperimentCodebase):

    def get_client_cmd(self, config, i, j, k, run, remote_exp_directory):
        client_id = i * config['client_nodes_per_server'] * config['client_processes_per_client_node'] + j * config['client_processes_per_client_node'] + k
        client_command = ' '.join([str(x) for x in [
            os.path.join(config['base_remote_bin_directory_nfs'],
                config['bin_directory_name'],
                config['client_bin_name']),
            '-s', config['client_experiment_length'],
            '-b', config['client_write_percentage'],
            '-e', config['client_write_percentage'] + config['client_read_percentage'],
            '-x', config['replication_protocol_settings']['message_transport_type'],
            '-i', client_id,
            '-m', config['replication_protocol'],
            '-c', config['network_config_file_name'],
            '-w', config['client_ramp_up'],
            ]])
        client_command = '%s 1> %s 2> %s' % (client_command,
            os.path.join(remote_exp_directory, config['out_directory_name'],
                'client-%d-%d-%d-stdout-%d.log' % (i, j, k, run)),
            os.path.join(remote_exp_directory, config['out_directory_name'],
                'client-%d-%d-%d-stderr-%d.log' % (i, j, k, run)))
        client_command = '(cd %s; %s) & ' % (remote_exp_directory, client_command)
        return client_command

    def get_replica_cmd(self, config, replica_id, process_id, group, run, remote_exp_directory):
        replica_command = ' '.join([str(x) for x in [
            os.path.join(config['base_remote_bin_directory_nfs'],
                config['bin_directory_name'], config['server_bin_name']),
            '-c', config['network_config_file_name'],
            '-i', replica_id,
            '-m', config['replication_protocol'],
            '-x', config['replication_protocol_settings']['message_transport_type']]])

        replica_command = '%s 1> %s 2> %s' % (replica_command,
                os.path.join(remote_exp_directory, config['out_directory_name'], 'replica-%d-stdout-%d.log' % (replica_id, run)),
                os.path.join(remote_exp_directory, config['out_directory_name'], 'replica-%d-stderr-%d.log' % (replica_id, run)))
        replica_command = 'cd %s; %s' % (remote_exp_directory, replica_command)
        return replica_command

    def prepare_local_exp_directory(self, config, config_file):
        local_exp_directory = super().prepare_local_exp_directory(config, config_file)
        with open(os.path.join(local_exp_directory, config['network_config_file_name']), 'w') as f:
            n = len(config['server_names'])
            print('f %d' % ((n - 1) // 2), file=f)
            for i in range(n):
                print('replica %s:%d %s:%d' % (config['server_names'][i],
                    config['server_port'],
                    config['server_names'][i],
                    config['server_rdma_port']), file=f)
        return local_exp_directory
    
    def prepare_remote_server_codebase(self, config, host, local_exp_directory, remote_out_directory):
        if ('message_transport_type' in config['replication_protocol_settings'] and config['replication_protocol_settings']['message_transport_type'] == 'rdma') or config['replication_protocol'] == 'abd-hybrid' or config['replication_protocol'] == 'abd-rdma':
            if not 'use_rxe' in config['replication_protocol_settings'] or not config['replication_protocol_settings']['use_rxe']:
                if 'rxe_cfg_path' in config: 
                    run_remote_command_sync('sudo %s stop' % config['rxe_cfg_path'], config['emulab_user'], host)
            else:
                interface =  get_exp_net_interface(config['emulab_user'], host)
                command = ('sudo %s stop ; '
                           'sudo %s start ; '
                           'sudo %s add %s') % (config['rxe_cfg_path'], config['rxe_cfg_path'], config['rxe_cfg_path'], interface)
                run_remote_command_sync(command, config['emulab_user'], host)

    def setup_nodes(self, config):
        pass

