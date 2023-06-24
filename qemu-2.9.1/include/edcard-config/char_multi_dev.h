#ifndef CHAR_MULTI_DEV_H
#define CHAR_MULTI_DEV_H

#define MAGIC 'm'
#define MIGRATE_START 			_IO(MAGIC, 0)
#define MIGRATE_STATE_SAVE 		_IOR(MAGIC, 1, unsigned char*)
#define MIGRATE_STATE_LOAD 		_IOW(MAGIC, 2, unsigned char*)
#define MIGRATE_END 			_IO(MAGIC, 3)
#define KEY_IMPORT              	_IOW(MAGIC, 4, unsigned char*)
#define RESET                   	_IO(MAGIC, 5)

#endif
