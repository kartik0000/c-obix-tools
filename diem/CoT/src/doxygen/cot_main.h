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
 * Contains source for @ref index.
 * @author Andrey Litvinov
 */
/**
 * @mainpage C oBIX Tools Full Reference Manual
 *
 * This documentation is intended only for contributors of C oBIX Tools project.
 * It describes all the source code and the internal architecture of the
 * project.
 * If you are interested only in the development of your own custom oBIX client
 * applications, than you should better use the documentation of oBIX Client
 * Library @a (libcot).
 *
 * @par Project Structure
 *
 * All project source files (including testing and examples) are kept at
 * <i>./src</i> folder. Full list of sources can be found at
 * <a href="files.html">Files tab</a>. Files are grouped in subfolders according
 * to the following logic:
 * @li <i>./src/adapters</i> - Contains device adapters, which are implemented
 * 							   based on C oBIX Client library. These adapters
 * 							   can be used as an example of library usage.
 * @li <i>./src/cleint</i>   - Sources of @link libcot_desc C oBIX Client
 *                             library @endlink
 * @li <i>./src/common</i>   - Sources of common tools used both by oBIX Client
 *                             library and oBIX Server.
 * @li <i>./src/server</i>	 - Sources of oBIX Server.
 *
 * There are also two more source subfolders with helper files:
 * @li <i>./src/doxygen</i>	 - Contains sources of some documentation pages
 * 							   (e.g. of this one). The rest part of
 * 							   documentation is built based on
 * @li <i>./src/test</i>	 - Contains unit test suite.
 *
 * Folder <i>./res</i> contains all project configuration files, including
 * settings for device adapters, server configuration and testing XML files.
 * The <i>./res</i> folder is not covered by this reference manual, but some
 * files here contain valuable comments:
 * @li <i>example_timer_config.xml</i> - Configuration file of example device
 * 			adapter. It explains all tags which can be used to configure some
 * 			general settings of oBIX client applications. Configuration files of
 * 			other device adapters from <i>./src/adapters</i> have the same
 * 			structure. These files follow <i>"<adapter_name>_config.xml"</i>
 * 			naming pattern.
 * @li <i>server_config.xml</i>	- Main configuration file of C oBIX server. All
 * 			settings here are described.
 * @li <i>server_test_device.xml</i> - This file can be used to put some test
 * 			data to the server, which will be loaded during server startup.
 *
 * Other <i>"server_*.xml"</i> files contain initial server's database and in
 * most cases should not be modified. However, each file has short description
 * of its content.
 *
 * @author Andrey Litvinov
 */
