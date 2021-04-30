/**
Copyright (c) 2007-2013 Alysson Bessani, Eduardo Alchieri, Paulo Sousa, and the authors indicated in the @author tags

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
package bftsmart.demo.bftinterface;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.locks.ReentrantLock;
import java.util.Arrays;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectInputStream;
import java.io.ObjectOutput;
import java.io.ObjectOutputStream;
import java.nio.ByteBuffer;
import java.security.Security;

import bftsmart.tom.MessageContext;
import bftsmart.tom.ServiceReplica;
import bftsmart.reconfiguration.ServerViewController;
import bftsmart.reconfiguration.util.TOMConfiguration;
import bftsmart.statemanagement.ApplicationState;
import bftsmart.statemanagement.StateManager;
import bftsmart.statemanagement.standard.StandardStateManager;
import bftsmart.tom.server.defaultservices.StateLog;
import bftsmart.tom.server.defaultservices.DefaultApplicationState;
import bftsmart.tom.server.defaultservices.CommandsInfo;
import bftsmart.tom.server.defaultservices.DiskStateLog;
import bftsmart.tom.ReplicaContext;
import bftsmart.tom.server.Executable;
import bftsmart.tom.server.Recoverable;
import bftsmart.tom.server.NoReplySingleExecutable;
import bftsmart.tom.util.TOMUtil;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.bouncycastle.jce.provider.BouncyCastleProvider;

/**
 * This class provides a basic state transfer protocol using the interface
 * 'NoReplySingleExecutable'.
 * @author Marcel Santos
 */
public class BftInterfaceServer implements Recoverable, NoReplySingleExecutable {
// public class BftInterfaceServer{
   
    private Logger logger = LoggerFactory.getLogger(this.getClass());
    
    protected ReplicaContext replicaContext;
    private TOMConfiguration config;
    private ServerViewController controller;
    private int checkpointPeriod;

    private ReentrantLock logLock = new ReentrantLock();
    private ReentrantLock hashLock = new ReentrantLock();
    private ReentrantLock stateLock = new ReentrantLock();
    
    private MessageDigest md;
        
    private StateLog log;
    private List<byte[]> commands = new ArrayList<>();
    private List<MessageContext> msgContexts = new ArrayList<>();
    
    private StateManager stateManager;

    private int counter = 0;
    private int iterations = 0;
    public long callbackHandle = 0;
    public ByteBuffer buffer;

    static{
       Security.addProvider(new BouncyCastleProvider());
    }
    
    public BftInterfaceServer(int id, long callbackHandle) {
        this.callbackHandle = callbackHandle;
        logger.info("start bft interface server in java...");
        try {
            md = TOMUtil.getHashEngine();
        } catch (NoSuchAlgorithmException ex) {
            logger.error("Failed to get message digest engine", ex);
        }
        logger.info("creating service replica...");
	    logger.info("Starting service replica with params:" + id + " " + this + " " + this);
        new ServiceReplica(id, this, this);
        logger.info("starting server " + id + " in the Java side!");

    }
    
    public void executeOrdered(byte[] command, MessageContext msgCtx) {
        
        executeOrdered(command, msgCtx, false);
        
    }
    
    private void executeOrdered(byte[] command, MessageContext msgCtx, boolean noop) {
        
        int cid = msgCtx.getConsensusId();
            
        if (!noop) {
            stateLock.lock();
            appExecuteOrdered(command, msgCtx);
            stateLock.unlock();
        }
        
        commands.add(command);
        msgContexts.add(msgCtx);
        
        if(msgCtx.isLastInBatch()) {
	        if ((cid > 0) && ((cid % checkpointPeriod) == 0)) {
	            // logger.debug("Performing checkpoint for consensus " + cid);
	            stateLock.lock();
	            byte[] snapshot = getSnapshot();
	            stateLock.unlock();
	            saveState(snapshot, cid);
	        } else {
	            saveCommands(commands.toArray(new byte[0][]), msgContexts.toArray(new MessageContext[0]));
	        }
			getStateManager().setLastCID(cid);
	        commands = new ArrayList<>();
            msgContexts = new ArrayList<>();
        }
    }
    
    private final byte[] computeHash(byte[] data) {
        byte[] ret = null;
        hashLock.lock();
        ret = md.digest(data);
        hashLock.unlock();

        return ret;
    }
    
    private StateLog getLog() {
       	initLog();
    	return log;
    }
    
    private void saveState(byte[] snapshot, int lastCID) {
        StateLog thisLog = getLog();

        logLock.lock();

        logger.debug("Saving state of CID " + lastCID);

        thisLog.newCheckpoint(snapshot, computeHash(snapshot), lastCID);
        thisLog.setLastCID(-1);
        thisLog.setLastCheckpointCID(lastCID);

        logLock.unlock();
        /*System.out.println("fiz checkpoint");
        System.out.println("tamanho do snapshot: " + snapshot.length);
        System.out.println("tamanho do log: " + thisLog.getMessageBatches().length);*/
        logger.debug("Finished saving state of CID " + lastCID);
    }

    private void saveCommands(byte[][] commands, MessageContext[] msgCtx) {
        
        if (commands.length != msgCtx.length) {
            logger.debug("----SIZE OF COMMANDS AND MESSAGE CONTEXTS IS DIFFERENT----");
            logger.debug("----COMMANDS: " + commands.length + ", CONTEXTS: " + msgCtx.length + " ----");
        }
        logLock.lock();

        int cid = msgCtx[0].getConsensusId();
        int batchStart = 0;
        for (int i = 0; i <= msgCtx.length; i++) {
            if (i == msgCtx.length) { // the batch command contains only one command or it is the last position of the array
                byte[][] batch = Arrays.copyOfRange(commands, batchStart, i);
                MessageContext[] batchMsgCtx = Arrays.copyOfRange(msgCtx, batchStart, i);
                log.addMessageBatch(batch, batchMsgCtx, cid);
            } else {
                if (msgCtx[i].getConsensusId() > cid) { // saves commands when the CID changes or when it is the last batch
                    byte[][] batch = Arrays.copyOfRange(commands, batchStart, i);
                    MessageContext[] batchMsgCtx = Arrays.copyOfRange(msgCtx, batchStart, i);
                    log.addMessageBatch(batch, batchMsgCtx, cid);
                    cid = msgCtx[i].getConsensusId();
                    batchStart = i;
                }
            }
        }
        
        logLock.unlock();
    }

    // @Override
    public ApplicationState getState(int cid, boolean sendState) {
        logLock.lock();
        ApplicationState ret = (cid > -1 ? getLog().getApplicationState(cid, sendState) : new DefaultApplicationState());

        // Only will send a state if I have a proof for the last logged decision/consensus
        //TODO: I should always make sure to have a log with proofs, since this is a result
        // of not storing anything after a checkpoint and before logging more requests        
        if (ret == null || (config.isBFT() && ret.getCertifiedDecision(this.controller) == null)) ret = new DefaultApplicationState();

        logger.info("Getting log until CID " + cid + ", null: " + (ret == null));
        logLock.unlock();
        return ret;
    }
    
    // @Override
    public int setState(ApplicationState recvState) {
        int lastCID = -1;
        if (recvState instanceof DefaultApplicationState) {
            
            DefaultApplicationState state = (DefaultApplicationState) recvState;
            
            logger.info("Last CID in state: " + state.getLastCID());
            
            logLock.lock();
            initLog();
            log.update(state);
            logLock.unlock();
            
            int lastCheckpointCID = state.getLastCheckpointCID();
            
            lastCID = state.getLastCID();

            logger.debug("I'm going to update myself from CID "
                    + lastCheckpointCID + " to CID " + lastCID);

            stateLock.lock();
            installSnapshot(state.getState());

            for (int cid = lastCheckpointCID + 1; cid <= lastCID; cid++) {
                try {
                    logger.debug("Processing and verifying batched requests for CID " + cid);

                    CommandsInfo cmdInfo = state.getMessageBatch(cid); 
                    byte[][] cmds = cmdInfo.commands; // take a batch
                    MessageContext[] msgCtxs = cmdInfo.msgCtx;
                    
                    if (cmds == null || msgCtxs == null || msgCtxs[0].isNoOp()) {
                        continue;
                    }
                    
                    for(int i = 0; i < cmds.length; i++) {
                    	appExecuteOrdered(cmds[i], msgCtxs[i]);
                    }
                } catch (Exception e) {
                    logger.error("Failed to process and verify batched requests",e);
                    if (e instanceof ArrayIndexOutOfBoundsException) {
                        logger.info("Last checkpoint, last consensus ID (CID): " + state.getLastCheckpointCID());
                        logger.info("Last CID: " + state.getLastCID());
                        logger.info("number of messages expected to be in the batch: " + (state.getLastCID() - state.getLastCheckpointCID() + 1));
                        logger.info("number of messages in the batch: " + state.getMessageBatches().length);
                    }
                }
            }
            stateLock.unlock();

        }

        return lastCID;
    }

    // @Override
    public void setReplicaContext(ReplicaContext replicaContext) {
        this.replicaContext = replicaContext;
        this.config = replicaContext.getStaticConfiguration();
        this.controller = replicaContext.getSVController();

        if (log == null) {
            checkpointPeriod = config.getCheckpointPeriod();
            byte[] state = getSnapshot();
            if (config.isToLog() && config.logToDisk()) {
                int replicaId = config.getProcessId();
                boolean isToLog = config.isToLog();
                boolean syncLog = config.isToWriteSyncLog();
                boolean syncCkp = config.isToWriteSyncCkp();
                log = new DiskStateLog(replicaId, state, computeHash(state), isToLog, syncLog, syncCkp);

                ApplicationState storedState = ((DiskStateLog) log).loadDurableState();
                if (storedState.getLastCID() > 0) {
                    setState(storedState);
                    getStateManager().setLastCID(storedState.getLastCID());
                }
            } else {
                log = new StateLog(this.config.getProcessId(), checkpointPeriod, state, computeHash(state));
            }
        }
        getStateManager().askCurrentConsensusId();
    }

    // @Override
    public StateManager getStateManager() {
    	if(stateManager == null)
    		stateManager = new StandardStateManager();
    	return stateManager;
    }
	
    private void initLog() {
    	if(log == null) {
    		checkpointPeriod = config.getCheckpointPeriod();
            byte[] state = getSnapshot();
            if(config.isToLog() && config.logToDisk()) {
            	int replicaId = config.getProcessId();
            	boolean isToLog = config.isToLog();
            	boolean syncLog = config.isToWriteSyncLog();
            	boolean syncCkp = config.isToWriteSyncCkp();
            	log = new DiskStateLog(replicaId, state, computeHash(state), isToLog, syncLog, syncCkp);
            } else
            	log = new StateLog(controller.getStaticConf().getProcessId(), checkpointPeriod, state, computeHash(state));
    	}
    }
          
    // @Override
    public byte[] executeUnordered(byte[] command, MessageContext msgCtx) {
        appExecuteUnordered(command, msgCtx);
        return null;
    }
    
    // @Override
    public void Op(int CID, byte[] requests, MessageContext msgCtx) {
        //Requests are logged within 'executeOrdered(...)' instead of in this method.
    }

    // @Override
    public void noOp(int CID, byte[][] operations, MessageContext[] msgCtx) {
         
        for (int i = 0; i < msgCtx.length; i++) {
            executeOrdered(operations[i], msgCtx[i], true);
        }
    }
            
    public void appExecuteUnordered(byte[] command, MessageContext msgCtx) {         
        iterations++;
        System.out.println("(" + iterations + ") Counter current value: " + counter);
    }
  
    public void appExecuteOrdered(byte[] command, MessageContext msgCtx) {
        iterations++;
        logger.info("in app execute ordered. array length: " + command.length);
        // for (int i = 0; i < command.length; ++i){
        //     System.out.print((int)command[i]);
        //     System.out.print(" ");
        // }
        // System.out.println();
        buffer = ByteBuffer.allocateDirect(command.length).put(command);
        logger.info("is direct? " + buffer.isDirect());
        bftRequestReceived(this);
        logger.info("buffer command executed! ");
    }
    
    @SuppressWarnings("unchecked")
    public void installSnapshot(byte[] state) {
        try {
            ByteArrayInputStream bis = new ByteArrayInputStream(state);
            ObjectInput in = new ObjectInputStream(bis);
            counter = in.readInt();
            in.close();
            bis.close();
        } catch (IOException e) {
            System.err.println("[ERROR] Error deserializing state: "
                    + e.getMessage());
        }
    }

    public byte[] getSnapshot() {
        try {
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            ObjectOutput out = new ObjectOutputStream(bos);
            out.writeInt(counter);
            out.flush();
            bos.flush();
            out.close();
            bos.close();
            return bos.toByteArray();
        } catch (IOException ioe) {
            System.err.println("[ERROR] Error serializing state: "
                    + ioe.getMessage());
            return "ERROR".getBytes();
        }
    }

    public native void bftRequestReceived(BftInterfaceServer request);

    // Used for testing purposes
    public static void main(String[] args){
        int i = Integer.parseInt(args[0]);
        new BftInterfaceServer(i, 0);
    }
}
