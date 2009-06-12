/**@file
 * Contains definitions for oBIX client library.
 *
 * @author Andrey Litvinov
 * @version 1.0.0
 */

/**@mainpage
 * The oBIX Client Library can be used by device drivers to post device data to
 * oBIX server and monitor value updates. In order to register new device,
 * server should support signUp feature which is not in the oBIX specification.
 * @n
 * @n @b Usage:
 * @n
 * The following string will compile application which uses oBIX Client Library:
 * @code
 * gcc -I<cot_headers> -L<cot_lib> -lcot-client <sources>
 * @endcode
 * where
 * - @a \<cot_headers> - Path to header files of libcot (usually it is
 * 						\<installation_prefix>/include/cot/).
 * - @a \<cot_lib>	  - Path to library binaries of libcot (usually it is
 * 						\<installation_prefix>/lib).
 * - @a \<sources>	  - Your source files to be compiled.
 *
 * @n
 * The typical usage of library (see example at example_timer.c):
 * @li Include obix_client.h header.
 *
 * @li Initialize library during driver startup. It can be done either by
 *     calling #obix_loadConfigFile() which will load settings from
 *     configuration file, or by #obix_loadConfig() with own generated XML
 *     settings structure.
 *
 * @li Open configured connection to oBIX server by calling
 *     #obix_openConnection().
 *
 * @li Generate an oBIX object for each device and register them at the server
 *     by calling #obix_registerDevice().
 *
 * @li If oBIX object, generated for the device, contains controlling values
 *     which can be changed outside, register listener for these values by
 *     calling #obix_registerListener(). Library start automatically polling
 *     changes of subscribed values and calls corresponding
 *     #obix_update_listener() when receives an updated value.
 *
 * @li When some update of device state is received by a driver, post new value
 *     to the oBIX server by calling #obix_writeValue().
 *
 * @li Call #obix_unregisterDevice() when driver detects that the device is not
 *     available any more (unplugged, turned off, etc.).
 *
 * @li Call #obix_dispose() when device driver is going down in order to close
 *     all connections and release resources reserved for communication with
 *     oBIX server(s).
 *
 * @author Andrey Litvinov
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
} OBIX_ERRORCODE;

typedef enum
{
	OBIX_T_BOOL,
	OBIX_T_INT,
	OBIX_T_REAL,
	OBIX_T_STR,
	OBIX_T_ENUM,
	OBIX_T_ABSTIME,
	OBIX_T_RELTIME,
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
 *
 * @param fileName Name of the configuration file.
 * @return #OBIX_SUCCESS if the library initialized successfully,
 *         error code otherwise.
 */
int obix_loadConfigFile(const char* fileName);

/**
 * Initializes library and loads connection setting from provided DOM
 * structure. Unlike #obix_loadConfigFile() it doesn't configure log system
 * of the library. Unconfigured log writes all messages to @a stdout.
 *
 * @todo Update description of obix_loadConfig() after adding log header to the
 * 		 library.
 *
 * @param config DOM structure representing a configuration XML.
 * @return #OBIX_SUCCESS if the library initialized successfully,
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
 * Opens connection with oBIX server.
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
 *       the device driver or external oBIX server users. @a writable attribute
 *       should be set to @a true; @a href attribute should have a valid URI,
 *       relative to the parent object.
 *
 * @n Example:
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
 * @note Parent object can specify @a href attribute but the oBIX server can
 *       modify it (for instance, add prefix of the device storage), thus URI
 *       can't be used to refer to the device record. Use device ID instead.
 *
 * @param connectionId ID of the connection which should be used.
 * @param obixData oBIX object representing the new device.
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
 * Overwrites value of the specified device parameter at the oBIX server.
 *
 * This function can be also used to change a value of any @a writable object
 * at the oBIX server. In that case @a 0 should be provided as @a deviceId and
 * @a paramUri should be relative to the server root.
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
 * @param newValue Text representation of the new value to be written.
 * @param dataType Type of data which is written to the server.
 * @return @a #OBIX_SUCCESS on success, negative error code otherwise.
 */
int obix_writeValue(int connectionId,
                    int deviceId,
                    const char* paramUri,
                    const char* newValue,
                    OBIX_DATA_TYPE dataType);

/**
 * Registers listener for device parameter updates.
 *
 * Overwrites existing listener if is called twice.
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
 *                 if changing parameter doesn't belong to devices registered
 *                 by this client.
 * @param listener Pointer to the listener function which would be invoked
 *                 every time when the subscribed parameter is changed.
 * @note @a listener method should be quick. Slow listener (especially if it
 *       waits for some resource) will block subsequent calls to listeners.
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

#endif /* OBIX_CLIENT_H_ */
