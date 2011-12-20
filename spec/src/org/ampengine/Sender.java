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
 * Sender
 *
 * @opt operations
 * @opt types
 *
 */

public interface Sender extends Link
{

    /**
     * indicates pending deliveries
     *
     * @param credits the number of pending deliveries
     * @todo is this absolute or cumulative?
     */
    public void offer(int credits);

    /**
     * Sends message data.
     *
     * @param delivery the delivery
     * @param bytes the message data
     * @param more a flag indicating if there is more data for this delivery
     */
    public void send(Delivery delivery, byte[] bytes, boolean more);

    /**
     * Abort a delivery.
     *
     * @param delivery the delivery to abort
     */
    public void abort(Delivery delivery);

}
