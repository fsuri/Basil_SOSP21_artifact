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
import org.bouncycastle.jce.provider.BouncyCastleProvider;


/**
 * Example client that updates a BFT replicated service (a counter).
 *
 */
public class BftInterfaceClient{

    int id;
    ServiceProxy serviceProxy;
    long callbackHandle;

    static{
       Security.addProvider(new BouncyCastleProvider());
    }

    public BftInterfaceClient(int id, long callbackHandle, String configHome) {

        this.id = id;
        this.callbackHandle = callbackHandle;
        System.out.println("BFTSMART-INTERFACE: Starting a new service proxy! config home: " + configHome);
        this.serviceProxy = new ServiceProxy(id, configHome);
        System.out.println("calling bft interface client constructor!");

    }

    public void startInterface(byte[] payload){
        System.out.println("BFTSMART-INTERFACE: InvokedOrdered at client side!");
        this.serviceProxy.invokeOrdered(payload);
        System.out.println("BFTSMART-INTERFACE: Finished ops on the service proxy!");
    }

    public void destructBftClient(){
        System.out.println("bft client " + id +  " destructed!");
        this.serviceProxy.close();
    }

    // public native void bftReplyReceived(byte[] reply, long callbackHandle);

}
