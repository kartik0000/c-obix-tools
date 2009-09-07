/* *****************************************************************************
 * Copyright (c) 2009 Andrey Litvinov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ****************************************************************************/
/**@file
 * @brief oBIX Client API.
 *
 * oBIX Client API simplifies implementing oBIX client applications, such as
 * device drivers. Library hides all network calls and allows accessing
 * data at the oBIX server without dealing with oBIX request formats. There is
 * also a possibility to subscribe for data updates on the server, which is
 * performed by library using oBIX Watch engine.
 *
 * @section obix-client-usage Usage
 *
 * The typical usage of the library in device adapter (see example at
 * example_timer.c):
 *
 * @li Include obix_client.h header.
 *
 * @li Initialize library during program startup. It can be done either by
 *     calling #obix_loadConfigFile() which will load settings from
 *     configuration file, or by #obix_loadConfig() with own generated XML
 *     settings structure.
 *
 * @li Open configured connection to oBIX server by calling
 *     #obix_openConnection().
 *
 * @li Generate an oBIX object for each device (for instance, one adapter can
 *     handle several devices of the same type) and register them at the server
 *     by calling #obix_registerDevice().
 *
 * @li If oBIX object, generated for the device, contains controlling values
 *     which can be changed outside (e.g. enabling/disabling boolean switch),
 *     register listener for these values by calling #obix_registerListener().
 *     Library starts polling changes of subscribed values and calls
 *     corresponding #obix_update_listener() every time when receives a new
 *     value.
 *
 * @li When some update of state variable is received by the driver from device,
 * 	   post the new value to the oBIX server by calling #obix_writeValue().
 *
 * @li Call #obix_unregisterDevice() when driver detects that the device is not
 *     available any more (unplugged, connection broken, etc.).
 *
 * @li When device driver is going down call #obix_dispose() in order to close
 *     all connections and release resources reserved for communication with
 *     oBIX server(s).
 *
 * @note In order to register a new device (using #obix_registerDevice), server
 * should support @a signUp feature which is not in the oBIX specification.
 * Currently @a signUp operation is supported by C oBIX Server included into
 * this distribution and oFMS (http://www.stok.fi/eng/ofms/index.html).
 * All other functions should work with any proper oBIX server implementation.
 * If not, please report the found error to the author of this distribution.
 *
 * @author Andrey Litvinov
 * @version 0.0.0
 */

#ifndef OBIX_CLIENT_H_
#define OBIX_CLIENT_H_

#include <ixml_ext.h>

/**
 * Error codes which are returned by library functions
 */
typedef enum
{
    /**Operation is completed successfully.*/
    OBIX_SUCCESS 				= 0,
    /**Function received wrong input argument.*/
    OBIX_ERR_INVALID_ARGUMENT 	= -1,
    /**Not enough memory to complete the operation.*/
    OBIX_ERR_NO_MEMORY			= -2,
    /**Library has invalid state.*/
    OBIX_ERR_INVALID_STATE		= -3,
    /**Allocated buffer for devices or listeners is full.
     * @todo Remove this error and enlarge arrays when needed.*/
    OBIX_ERR_LIMIT_REACHED		= -4,
    /**Error in communication with server.*/
    OBIX_ERR_BAD_CONNECTION		= -5,
    /**Reserved for uncaught errors. If such error occurs, this is a bug.*/
    OBIX_ERR_UNKNOWN_BUG		= -100,
    /**Error inside HTTP communication module.*/
    OBIX_ERR_HTTP_LIB			= -6,
    /**oBIX server returned an error object.*/
    OBIX_ERR_SERVER_ERROR		= -7
} OBIX_ERRORCODE;

/**
 * Standard oBIX data types. Used in #obix_writeValue().
 */
typedef enum
{
    /** Boolean data type (bool). Possible values: @a "true" or @a "false". */
    OBIX_T_BOOL,
    /** Integer data type (int). Possible values are defined by @a xs:long. */
    OBIX_T_INT,
    /** Real data type (real). Possible values are defined by @a xs:double. */
    OBIX_T_REAL,
    /** String data type (str). */
    OBIX_T_STR,
    /** Enumeration data type (enum). Possible values are defined by associated
     *  @a range object. */
    OBIX_T_ENUM,
    /** Time data type (abstime). Represents an absolute point in time. Value
     * format is defined by @a xs:dateTime. */
    OBIX_T_ABSTIME,
    /** Time data type (reltime). Represents time interval. Value format is
     * defined by @a xs:duration. */
    OBIX_T_RELTIME,
    /** URI data type (uri). Almost like a string, but contains valid URI. */
    OBIX_T_URI
} OBIX_DATA_TYPE;

/**
 * Callback function, which is invoked when subscribed value is changed at the
 * oBIX server.
 *
 * ID arguments of the listener can be used to define which parameter was
 * updated in case when one function is registered to handle updates of several
 * parameters.
 *
 * @see obix_registerListener()
 *
 * @param connectionId ID of the connection from which the update is received.
 * @param deviceId   ID of the device whose parameter was changed. If the
 *                   parameter doesn't belong to any device which was
 *                   registered by current client, than @a 0 will be passed.
 * @param listenerId ID of the listener which receives the event.
 * @param newValue   New value of the parameter.
 * @return The listener should return #OBIX_SUCCESS if the event was handled
 *         properly. Any other returned value will be considered by library as
 *         an error.
 */
typedef int (*obix_update_listener)(int connectionId,
                                    int deviceId,
                                    int listenerId,
                                    const char* newValue);

/**
 * Initializes library and loads connection setting from XML file.
 * Also sets up the logging system of the library.
 * The format of the configuration file can be found at example_timer_config.xml
 *
 * @param fileName Name of the configuration file.
 * @return #OBIX_SUCCESS if the library initialized successfully,
 *         error code otherwise.
 */
int obix_loadConfigFile(const char* fileName);

/**
 * Initializes library and loads connection setting from provided DOM
 * structure. Unlike #obix_loadConfigFile() it doesn't configure log system
 * of the library. It can be configured manually using #config_log() or
 * log_utils.h functions. By default, all messages (including debug ones) are
 * written to @a stdout.
 * The format of the configuration file can be found at example_timer_config.xml
 *
 * @param config DOM structure representing a configuration XML.
 * @return #OBIX_SUCCESS if the library is initialized successfully,
 *         error code otherwise.
 */
int obix_loadConfig(IXML_Element* config);

/**
 * Releases all resources allocated by library. All registered listeners and
 * devices are unregistered, all open connections are closed.
 *
 * @return #OBIX_SUCCESS if operation is completed successfully, error code
 * 		   otherwise.
 */
int obix_dispose();

/**
 * Opens connection to the oBIX server.
 *
 * @param connectionId Connection ID which was specified in the loaded
 * 					   configuration file.
 * @return #OBIX_SUCCESS if connection is opened successfully, error code
 *         otherwise.
 */
int obix_openConnection(int connectionId);

/**
 * Closes specified connection releasing all used resources.
 * Also unregisters all devices and listeners which were registered using this
 * connection.
 *
 * @param connectionId ID of the connection which should be closed.
 * @return #OBIX_SUCCESS on success, error code otherwise.
 */
int obix_closeConnection(int connectionId);

/**
 * Posts the provided device information to the oBIX server.
 *
 * @note Input data is not tested to conform with oBIX specification.
 *       Is is strongly recommended to provide @a displayName attribute
 *       to every object. Also attributes @a href and @a writable are
 *       obligatory for all device parameters which are going to be changed by
 *       the device driver or external oBIX server users:
 *       - @a writable attribute should be set to @a true.
 *       - @a href attribute of the parent object should be a valid URI,
 *         relative to the server root (start with "/").
 *       - @a href attributes of all child objects should have a valid URIs,
 *         relative to the parent object.
 *
 * @n @b Example:
 * @code
 * <obj name="light" href="/kitchen/light/" displayName="Kitchen lights">
 *     <bool name="switch"
 *         href="switch/"
 *         displayName="Switch"
 *         is="obix:bool"
 *         val="true"
 *         writable="true"/>
 * </obj>
 * @endcode
 *
 * @note Parent object should specify @a href attribute but the oBIX server is
 *       free to modify it (for instance, add prefix of the device storage),
 *       thus the URI can't be used to refer to the device record. Use the
 *       assigned device ID instead.
 *
 * @param connectionId ID of the connection which should be used.
 * @param obixData oBIX object representing a new device.
 * @return @li >0 ID of the created device record;
 *         @li <0 error code.
 */
int obix_registerDevice(int connectionId, const char* obixData);

/**
 * Removes device record from the oBIX server.
 * Also removes all listeners which were registered for this device.
 *
 * @param connectionId ID of the connection which should be used.
 * @param deviceId ID of the device which should be removed.
 * @return #OBIX_SUCCESS if the device record is removed successfully,
 * 		   error code otherwise.
 */
int obix_unregisterDevice(int connectionId, int deviceId);

/**
 * Reads value of the specified parameter from the oBIX server.
 *
 * This function can be used to read value of any object on the server: It can
 * be some parameter of device which was registered either by the same client,
 * or by any other device driver. It can be also any value of the server
 * settings.
 *
 * @n @b Example: Server contains the following information:
 * @code
 * <obj name="device1" href="/obix/device1/">
 *   <obj name="configure" href="conf/" />
 *     <str name="name" href="name" val="My Device"/>
 *   </obj>
 * </obj>
 * @endcode
 * There are two ways to read the value of @a "name" string of @a "device1":
 * - If this device was registered by the same client earlier, than the client
 *   should use device ID assigned to this device and provide parameter URI
 *   relative to the root of device data (in current example it is @a
 *   "conf/name").
 * - In case if that device was registered by someone else, than @a 0 should be
 *   used instead device ID + full URI of the required parameter (in this
 *   example it is @a "/obix/device1/conf/name").
 *
 * @note This method is used to read only @a val attribute of some object, but
 *       not the whole object itself. If you need to read the whole oBIX object
 *       then use #obix_read instead.
 *
 * @note Although there is no need for this function in the normal workflow (see
 *       @ref obix-client-usage), it still can be used, for instance, during
 *       initialization phase for obtaining some data from the server. Usage of
 *       this function for periodical reading of some object is not efficient
 *       and should be avoided. Use #obix_registerListener instead.
 *
 * @param connectionId ID of the connection which should be used.
 * @param deviceId ID of the device whose parameter should be read or @a 0
 *                 if the parameter doesn't belong to devices registered by
 *                 this client.
 * @param paramUri URI of the parameter. It should be either relative to the
 *                 device record like it was provided during device
 *                 registration, or relative to the server root if the
 *                 parameter doesn't belong to devices registered by this
 *                 client.
 *
 * @param output If read command executed successfully the attribute's value is
 *               stored here.
 * @return @a #OBIX_SUCCESS on success, negative error code otherwise.
 */
int obix_readValue(int connectionId,
                   int deviceId,
                   const char* paramUri,
                   char** output);

/**
 * Overwrites value of the specified device parameter at the oBIX server.
 *
 * This function can be also used to change a value of any @a writable object
 * at the oBIX server.
 *
 * @param connectionId ID of the connection which should be used.
 * @param deviceId ID of the device whose parameter should be changed or @a 0
 *                 if the parameter doesn't belong to devices registered by
 *                 this client.
 * @param paramUri URI of the parameter. It should be either relative to the
 *                 device record like it was provided during device
 *                 registration, or relative to the server root if changing
 *                 parameter doesn't belong to devices registered by this
 *                 client.
 * @param newValue Text representation of the new value to be written. It should
 *                 be a new value for the @a val attribute of the oBIX object on
 *                 the server, not the whole object.
 * @note  Only value of an object (@a val attribute) can be written using this
 * 		  method. It's not possible to overwrite a whole oBIX object on the
 *        server.
 * @param dataType Type of data which is written to the server.
 * @return @a #OBIX_SUCCESS on success, negative error code otherwise.
 *
 * @see #obix_readValue() for the usage example.
 */
int obix_writeValue(int connectionId,
                    int deviceId,
                    const char* paramUri,
                    const char* newValue,
                    OBIX_DATA_TYPE dataType);

/**
 * Reads the whole oBIX object from the server and returns it as a DOM
 * structure.
 *
 * This function can be used to read any object on the server. It can be some
 * object which was registered either by the same client, or by any other device
 * driver. It can be also any server's own object.
 *
 * @n @b Example: Server contains the following information:
 * @code
 * <obj name="device1" href="/obix/device1/">
 *   <obj name="configure" href="conf/" />
 *     <str name="name" href="name" val="My Device"/>
 *   </obj>
 * </obj>
 * @endcode
 * There are two options to read the @a "configure" object of @a "device1":
 * - If this device was registered by the same client earlier, than the client
 *   should use device ID assigned to this device and provide parameter URI
 *   relative to the root of device data (in current example it is @a
 *   "conf/").
 * - In case if that device was registered by someone else, than @a 0 should be
 *   used instead device ID + full URI of the required object (in this
 *   example it is @a "/obix/device1/conf/").
 *
 * If the whole object "device1" should be read and it was previously published
 * by the same client, then the client should provide device ID assigned to this
 * device and @a NULL as paramUri.
 *
 * @note Although there is no need for this function in the normal workflow (see
 *       @ref obix-client-usage) it still can be used, for instance, during
 *       initialization phase for obtaining some data from the server. Usage of
 *       this function for periodical reading of some object is not efficient
 *       and should be avoided. Use #obix_registerListener instead.
 *
 * @param connectionId ID of the connection which should be used.
 * @param deviceId ID of the device whose data should be read or @a 0 if the
 *                 object doesn't belong to devices registered by this client.
 * @param paramUri URI of the object. It should be either relative to the
 *                 device record like it was provided during device
 *                 registration, or relative to the server root if the
 *                 object doesn't belong to devices registered by this
 *                 client.
 *
 * @param output If read command executed successfully the DOM representation of
 *               the read object is stored here.
 * @return @a #OBIX_SUCCESS on success, negative error code otherwise.
 */
int obix_read(int connectionId,
              int deviceId,
              const char* paramUri,
              IXML_Element** output);

/**
 * Registers listener for device parameter updates.
 *
 * Overwrites existing listener if it is called twice for the same parameter.
 *
 * This method can be also used to subscribe for the updates of any other
 * objects stored at the oBIX server. In that case @a 0 should be provided as
 * @a deviceId and @a paramUri should be relative to the server root.
 *
 * @param connectionId ID of the connection which should be used.
 * @param deviceId ID of the device whose parameter should be monitored or
 *                 @a 0 if the parameter doesn't belong to devices registered
 *                 by this client.
 * @param paramUri URI of the parameter which should be monitored. It should be
 *                 either relative to the device record like it was provided
 *                 during device registration, or relative to the server root
 *                 if the parameter doesn't belong to devices registered
 *                 by this client.
 * @param listener Pointer to the listener function which would be invoked
 *                 every time when the subscribed parameter is changed.
 * @note @a listener method should be quick. Slow listener (especially if it
 *       waits for some resource) will block subsequent calls to all listeners.
 * @return @li >=0 ID of the created listener;
 *         @li <0 error code.
 */
int obix_registerListener(int connectionId,
                          int deviceId,
                          const char* paramUri,
                          obix_update_listener listener);

/**
 * Unregisters listener of device parameter updates.
 *
 * @param connectionId ID of the connection which should be used.
 * @param deviceId   ID of the device whose parameter is now monitored or
 *                   @a 0 if the parameter doesn't belong to devices registered
 *                   by this client.
 * @param listenerId ID of listener to be removed.
 * @return #OBIX_SUCCESS if the listener is removed successfully, error code
 *         otherwise.
 */
int obix_unregisterListener(int connectionId,
                            int deviceId,
                            int listenerId);

/**
 * Represents a Batch object. oBIX Batch allows combining several commands in
 * one request to the server, thus reducing response time and network load.
 *
 * <b>General Usage:</b> @n
 * - Create new Batch instance using #obix_batch_create();
 * - Add commands to batch using #obix_batch_read(), #obix_batch_readValue() or
 *   #obix_batch_writeValue();
 * - Send Batch object to the server by calling #obix_batch_send();
 * - An instance of #oBIX_BatchResult will be generated for each command in
 *   Batch, containing execution results. These results can be obtained using
 *   #obix_batch_getResult();
 * - Free Batch object with #obix_batch_free().
 */
typedef struct _oBIX_Batch oBIX_Batch;

/**
 * Contains outputs of the command, which was executed in a
 * @ref #oBIX_Batch "Batch".
 */
typedef struct _oBIX_BatchResult
{
	/**
	 * Return value of the executed command. It is identical to the return
	 * value of the corresponding command executed without Batch.
	 */
    int status;
    /**
     * String value returned by the function, if available (e.g. for
     * #obix_batch_readValue()).
     */
    char* value;
    /**
     * XML object returned by the function, if available (e.g. for
     * #obix_batch_read()).
     */
    IXML_Element* obj;
}
oBIX_BatchResult;

/**@name oBIX Batch operations
 * @{
 */
/**
 * Creates a new Batch instance. oBIX Batch contains several operations
 * which can be executed by one request to the server. Commands are executed at
 * the server in the order they were added to the Batch.
 *
 * @param connectionId ID of the connection for which batch is created.
 * @return New Batch instance or @a NULL on error.
 */
oBIX_Batch* obix_batch_create(int connectionId);

/**
 * Adds readValue operation to the provided Batch. When Batch is executed,
 * this operation will act like #obix_readValue(). Read value will be
 * stored at the corresponding oBIX_BatchResult::value.
 *
 * @param batch    Batch object where readValue operation should be added to.
 * @param deviceId ID of the device whose parameter should be read.
 * @param paramUri Uri of the parameter which should be read.
 * @return @li @a >0 - ID of the added command. IDs are assigned according to
 *         the order of adding commands to the Batch. Thus ID of the first added
 *         command will be @a 1, ID of the second - @a 2, and so on.
 *         @li @a <0 - Error code indicating that adding command to the Batch
 *         failed.
 *         @b Note that this is not the return code of the
 *         execution of read command: It will be stored in the corresponding
 *         oBIX_BatchResult::status after the whole Batch is executed.
 *
 * @note Results of the previous execution of the Batch will become
 *       unavailable after calling this method.
 *
 * @see #obix_readValue()
 */
int obix_batch_readValue(oBIX_Batch* batch,
                         int deviceId,
                         const char* paramUri);
/**
 * Adds read operation to the provided Batch. When Batch is executed, this
 * operation will act like #obix_read(). Read object will be stored at the
 * corresponding oBIX_BatchResult::obj.
 *
 * @param batch    Batch object where read operation should be added to.
 * @param deviceId ID of the device whose parameter should be read.
 * @param paramUri Uri of the parameter which should be read.
 * @return @li @a >0 - ID of the added command. IDs are assigned according to
 *         the order of adding commands to the Batch. Thus ID of the first added
 *         command will be @a 1, ID of the second - @a 2, and so on.
 *         @li @a <0 - Error code indicating that adding command to the Batch
 *         failed.
 *         @b Note that this is not the return code of the
 *         execution of read command: It will be stored in the corresponding
 *         oBIX_BatchResult::status after the whole Batch is executed.
 *
 * @note Results of the previous execution of the Batch will become
 *       unavailable after calling this method.
 *
 * @see #obix_read()
 */
int obix_batch_read(oBIX_Batch* batch,
                    int deviceId,
                    const char* paramUri);

/**
 * Adds writeValue operation to the provided Batch. When Batch is executed, this
 * operation will act like #obix_writeValue().
 *
 * @param batch    Batch object where writeValue operation should be added to.
 * @param deviceId ID of the device whose parameter's value should be written.
 * @param paramUri Uri of the parameter which should be written.
 * @param newValue Text representation of the new value to be written. It should
 *                 be a new value for the @a val attribute of the oBIX object on
 *                 the server, not the whole object.
 * @param dataType Type of data which is written to the server.
 * @return @li @a >0 - ID of the added command. IDs are assigned according to
 *         the order of adding commands to the Batch. Thus ID of the first added
 *         command will be @a 1, ID of the second - @a 2, and so on.
 *         @li @a <0 - Error code indicating that adding command to the Batch
 *         failed.
 *         @b Note that this is not the return code of the
 *         execution of write command: It will be stored in the corresponding
 *         oBIX_BatchResult::status after the whole Batch is executed.
 *
 * @note  Only value of an object (@a val attribute) can be written using this
 * 		  method. It's not possible to overwrite a whole oBIX object on the
 *        server.
 * @note Results of the previous execution of the Batch will become
 *       unavailable after calling this method.
 *
 * @see #obix_writeValue()
 */
int obix_batch_writeValue(oBIX_Batch* batch,
                          int deviceId,
                          const char* paramUri,
                          const char* newValue,
                          OBIX_DATA_TYPE dataType);

/**
 * Removes command with specified ID from the Batch object.
 *
 * @param batch Batch object, from which the command should be removed.
 * @param commandId ID of the command which should be removed.
 * @return @li #OBIX_SUCCESS if the command is removed successfully;
 *         @li #OBIX_ERR_INVALID_ARGUMENT if batch is @a NULL;
 *         @li #OBIX_ERR_INVALID_STATE if no command with specified ID is
 *         found in the Batch.
 *
 * @note Results of the previous execution of the Batch will become
 *       unavailable after calling this method.
 */
int obix_batch_removeCommand(oBIX_Batch* batch, int commandId);

/**
 * Sends the Batch object to the oBIX server.
 * After successful execution, a set of #oBIX_BatchResult objects will be
 * generated containing execution results for each command in Batch.
 * Same Batch object can be sent to the server several times. In that case
 * previous results will be freed and new one will be generated.
 *
 * @param batch Batch object to be sent.
 * @return #OBIX_SUCCESS if the Batch object is sent successfully and server
 *         returned no errors. Error code is returned in case if at least one
 *         command in batch caused error.
 */
int obix_batch_send(oBIX_Batch* batch);

/**
 * Returns execution results of the command from specified Batch object.
 *
 * @param batch Batch object in which the command was executed.
 * @param commandId ID of the command whose results should be returned.
 * @return An instance of oBIX_BatchResult, containing returned error code of
 *         the executed command and other return values if any.
 *
 * @note Do not free the returned values. They will be freed automatically
 *       by next execution of #obix_batch_send() or #obix_batch_free().
 */
const oBIX_BatchResult* obix_batch_getResult(oBIX_Batch* batch, int commandId);

/**
 * Releases memory allocated for the provided Batch object.
 * Batch object will become unusable after calling this method.
 *
 * @param batch Batch object to be deleted.
 */
void obix_batch_free(oBIX_Batch* batch);
/** @} */

#endif /* OBIX_CLIENT_H_ */
