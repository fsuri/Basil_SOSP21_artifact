import abc
import os
import shutil
from utils.remote_util import *

class ExperimentCodebase(abc.ABC):

    @abc.abstractmethod
    def get_client_cmd(self, config, i, j, k, run, local_exp_directory,
            remote_exp_directory):
        pass

    @abc.abstractmethod
    def get_replica_cmd(self, config, replica_id, local_exp_directory,
            remote_exp_directory):
        pass

    def prepare_local_exp_directory(self, config, config_file):
        exp_directory = get_timestamped_exp_dir(config)
        os.makedirs(exp_directory)
        shutil.copy(config_file, os.path.join(exp_directory,
        os.path.basename(config_file)))
        return exp_directory

    @abc.abstractmethod
    def prepare_remote_server_codebase(self, config, server_host, local_exp_directory, remote_out_directory):
        pass

    @abc.abstractmethod
    def setup_nodes(self, config):
        pass

from lib.tupaq_codebase import *
from lib.rdma_repl_codebase import *
from lib.morty_codebase import *
from lib.indicus_codebase import *

__BUILDERS__ = {
    "tupaq": TupaqCodebase(),
    "rdma-repl": RdmaReplCodebase(),
    "morty": MortyCodebase(),
    "indicus": IndicusCodebase()
}

def get_client_cmd(config, i, j, k, run, local_exp_directory,
        remote_exp_directory):
    return __BUILDERS__[config['codebase_name']].get_client_cmd(config, i, j,
            k, run, local_exp_directory, remote_exp_directory)

def get_replica_cmd(config, i, k, group, run, local_exp_directory,
        remote_exp_directory):
    return __BUILDERS__[config['codebase_name']].get_replica_cmd(config,
            i, k, group, run, local_exp_directory, remote_exp_directory)

def prepare_local_exp_directory(config, config_file):
    return __BUILDERS__[config['codebase_name']].prepare_local_exp_directory(config, config_file)

def prepare_remote_server_codebase(config, server_host, local_exp_directory, remote_out_directory):
    return __BUILDERS__[config['codebase_name']].prepare_remote_server_codebase(config, server_host, local_exp_directory, remote_out_directory)

def setup_nodes(config):
    return __BUILDERS__[config['codebase_name']].setup_nodes(config)
