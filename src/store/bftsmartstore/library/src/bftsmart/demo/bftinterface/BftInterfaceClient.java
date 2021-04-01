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

import bftsmart.tom.AsynchServiceProxy;
import bftsmart.tom.RequestContext;
import bftsmart.tom.core.messages.TOMMessage;
import bftsmart.tom.core.messages.TOMMessageType;
import bftsmart.communication.client.ReplyListener;


/**
 * Example client that updates a BFT replicated service (a counter).
 * 
 */
public class BftInterfaceClient{

    int id;
    AsynchServiceProxy serviceProxy;
    long callbackHandle;
    int numberOfOps;
    int requestSize;
    int interval;
    byte[] request;
    TOMMessageType reqType;
    boolean verbose;
    int rampup = 3000;

    public BftInterfaceClient(int id, long callbackHandle){
        this(id, false, false, callbackHandle);
    }

    public BftInterfaceClient(int id, boolean readOnly, boolean verbose, long callbackHandle) {

        this.id = id;
        this.serviceProxy = new AsynchServiceProxy(id);
        this.reqType = (readOnly ? TOMMessageType.UNORDERED_REQUEST : TOMMessageType.ORDERED_REQUEST);
        this.verbose = verbose;
        this.callbackHandle = callbackHandle;

    }

    public void startInterface(byte[] payload){
        this.serviceProxy.invokeAsynchRequest(payload, new ReplyListener() {
            private int replies = 0;
            @Override
            public void reset() {
                if (verbose) System.out.println("[RequestContext] The proxy is re-issuing the request to the replicas");
                replies = 0;
            }
            @Override
            public void replyReceived(RequestContext context, TOMMessage reply) {
                StringBuilder builder = new StringBuilder();
                builder.append("[RequestContext] id: " + context.getReqId() + " type: " + context.getRequestType());
                builder.append("[TOMMessage reply] sender id: " + reply.getSender() + " Hash content: " + Arrays.toString(reply.getContent()));
                if (verbose) System.out.println(builder.toString());

                replies++;

                bftReplyReceived(reply.getContent(), callbackHandle);

                double q = Math.ceil((double) (serviceProxy.getViewManager().getCurrentViewN() + serviceProxy.getViewManager().getCurrentViewF() + 1) / 2.0);

                if (replies >= q) {
                    if (verbose) System.out.println("[RequestContext] clean request context id: " + context.getReqId());
                    serviceProxy.cleanAsynchRequest(context.getOperationId());
                }
            }
        }, this.reqType);
    }

    public void destructBftClient(){
        this.serviceProxy.close();
    }

    public native void bftReplyReceived(byte[] reply, long callbackHandle);

}

