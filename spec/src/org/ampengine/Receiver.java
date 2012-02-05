/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
package org.ampengine;


/**
 * Receiver
 *
 * @opt operations
 * @opt types
 *
 */

public interface Receiver extends Link
{

    /**
     * issue the specified number of credits
     */
    public void flow(int credits);

    /**
     * @return the next incoming delivery or null if there is none
     */
    public Delivery next();

    /**
     * Receive message data for the current delivery.
     *
     * @param bytes the destination array where the message data is written
     * @param offset the index to begin writing into the array
     * @param size the maximum number of bytes to write
     *
     * @return the number of bytes written or -1 if there is no more
     *         message data for the current delivery
     */
    public int recv(byte[] bytes, int offset, int size);

}
