#ifndef _GETCHAR_NB_H_
#define _GETCHAR_NB_H_

int getchar_nonblock(void);

void nonblocking(bool enable);

// example:
// nonblocking( true );
// getchar_nonblock();
// nonblocking( false );


#endif // _GETCHAR_NB_H_

