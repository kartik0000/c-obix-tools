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
/**
 * @file
 * Doxygen documentation source file.
 * Contains source for @ref libcot_desc.
 */
/**
 * @page libcot_desc oBIX Client Library Overview
 *
 * oBIX Client Library (libcot) is a small library written in C, which allows
 * creating lightweight oBIX client applications such as device adapters.
 * @n
 * Library API consists of obix_client.h which declares oBIX Client API and
 * several utility header files which provides helper tools commonly needed by
 * oBIX client applications:
 * @li ptask.h - Allows scheduling tasks to be executed in a separate thread;
 * @li obix_utils.h - Contains names of most essential oBIX objects, attributes,
 *     etc.
 * @li xml_config.h - Helps to load application settings from XML file;
 * @li log_utils.h - Provides interface to the logging utility which is used by
 *     the whole library.
 * @li ixml_ext.h - Contains utilities for manipulating XML DOM structures.
 *
 * All parts of the library produce log messages which give better understanding
 * of what happens inside and help to investigate occurred problems. By default
 * the logging system writes everything (including all debug messages) to
 * @a stdout which can be inconvenient. Log settings can be changed either
 * manually (using functions defined at log_utils.h) or during oBIX Client API
 * initialization (using #obix_loadConfigFile()).
 *
 * The example usage of the library can be found at example_timer.c
 *
 * @par Compilation
 *
 * The following string will compile application which uses oBIX Client Library:
 * @code
 * gcc -I<cot_headers> -L<cot_lib> -lcot-client <source> -o <output_name>
 * @endcode
 * where
 * - @a \<cot_headers> - Path to header files of @a libcot (usually it is
 * 						\<installation_prefix>/include/cot/).
 * - @a \<cot_lib>	  - Path to library binaries of libcot (usually it is
 * 						\<installation_prefix>/lib).
 * - @a \<sources>	  - Your source files to be compiled.
 * - @a \<output_name> - Name of the output binary.
 *
 * @author Andrey Litvinov
 */
