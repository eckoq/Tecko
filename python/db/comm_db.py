#_*_ coding:utf8 -*- #
import MySQLdb,threading
db_lock = threading.Lock()


class DbError(Exception):
    def __init__(self, str):
        self.value = str
    def __str__(self):
        return repr(self.value)
        
class DbBase():
    def __init__(self, db_host, db_port, db_user, db_pass, db_name, charset="utf8"):
        self.db_host = db_host
        self.db_port = db_port
        self.db_user = db_user
        self.db_pass = db_pass
        self.db_name = db_name
        self.init = False
        self.charset = charset
        '''
        try:
            self.conn = MySQLdb.connect(db_host, db_user, db_pass, db_name)
            self.c = self.conn.cursor()
        except MySQLdb.Error, e:
            raise DbError("Error %d:%s" %(e.args[0], e.args[1]))
        '''
    def __del__(self):  
        '''
        self.c.close()
        self.conn.close()
        '''
        pass
        
    def Connect(self):
        try:
            conn = MySQLdb.connect(self.db_host, self.db_user, self.db_pass, self.db_name, port=self.db_port, use_unicode=False, charset=self.charset)
            c = conn.cursor()
            self.init = True
            return conn, c
        except MySQLdb.Error, e:
            self.init = False
            raise DbError("Error %d:%s" %(e.args[0], e.args[1]))

    def disConnect(self, conn, c):
        pass
    
    def execute(self, sql, bget, bmulti, charset = None):
        conn = None
        c = None
        try:
            conn, c = self.Connect()    
            
            if charset != None:
                conn.set_character_set(charset)

            if not bget:
                db_lock.acquire()
            
            c.execute(sql)
            conn.commit()

            if not bget:
                conn.commit()
                c.close()
                conn.close()
                db_lock.release()
                return 0
                
            if bmulti:
                rows = c.fetchall()
                c.close()
                conn.close()
                return rows
            else:
                row = c.fetchone()
                c.close()
                conn.close()
                return row
        except MySQLdb.Error, e:
            if db_lock.locked():
                db_lock.release()
            if self.init:
                c.close()
                conn.close()
            raise DbError("Error %d:%s" %(e.args[0], e.args[1]))
    
    def submit(self):
        try:
            '''
            self.conn.commit()
            '''
            return 0
        except MySQLdb.Error, e:
            raise DbError("Error %d:%s" %(e.args[0], e.args[1]))

class db_op(DbBase):
    def __init__(self, db_host, db_port, db_user, db_pass, db_name):
        DbBase.__init__(self, db_host, db_port, db_user, db_pass, db_name)
	        
    def __del__(self):
        DbBase.__del__(self)
	 
    def execute_sql(self, sql):
        try:    
            ret_v = self.execute(sql, True, True, self.charset)
            return 0, ret_v    
			
        except DbError, e:
            return -1, e.value  
            
