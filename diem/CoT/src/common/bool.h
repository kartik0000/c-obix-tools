/** @file
 *
 * Defines boolean type.
 * Just in case if nobody defined it before.
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#ifndef BOOL_H_
#define BOOL_H_

/** Boolean data type which so natural for all programmers. */
typedef int BOOL;

#ifndef TRUE
/** That's @a true. */
#define TRUE 1
#endif

#ifndef FALSE
/** This is @a false. */
#define FALSE 0
#endif

#endif /* BOOL_H_ */
