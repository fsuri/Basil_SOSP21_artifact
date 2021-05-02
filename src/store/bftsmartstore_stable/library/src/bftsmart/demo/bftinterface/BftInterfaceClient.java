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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.security.Security;

import bftsmart.tom.ServiceProxy;
import bftsmart.reconfiguration.util.Configuration;
import org.bouncycastle.jce.provider.BouncyCastleProvider;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


/**
 * Example client that updates a BFT replicated service (a counter).
 *
 */
public class BftInterfaceClient{

    int id;
    ServiceProxy serviceProxy;
    long callbackHandle;
    private Logger logger = LoggerFactory.getLogger(this.getClass());

    static{
       Security.addProvider(new BouncyCastleProvider());
    }

    public BftInterfaceClient(int id, long callbackHandle, String configHome, String configBase) {
        Configuration.configBase = configBase;
        this.id = id;
        this.callbackHandle = callbackHandle;
        logger.info("BFTSMART-INTERFACE: Starting a new service proxy! config home: " + configHome);
        this.serviceProxy = new ServiceProxy(id, configHome);
        logger.info("calling bft interface client constructor!");

    }

    public void startInterface(byte[] payload){
        logger.info("BFTSMART-INTERFACE: InvokedOrdered at client side!");
        this.serviceProxy.invokeOrdered(payload);
        logger.info("BFTSMART-INTERFACE: Finished ops on the service proxy!");
    }

    public void destructBftClient(){
        logger.info("bft client " + id +  " destructed!");
        this.serviceProxy.close();
    }

    // public native void bftReplyReceived(byte[] reply, long callbackHandle);

}
