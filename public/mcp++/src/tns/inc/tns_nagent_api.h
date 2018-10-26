#ifndef _TNS_AGENT_API_H_
#define _TNS_AGENT_API_H_

#include <set>
#include <string>

namespace storage
{
    namespace tns
    {
        namespace nagent
        {
            class CRingBuffer;
        }

        namespace asn
        {
            class StorageMessage;
        }

        namespace agent_api
        {
            const int BUF_SIZE = 32*1024*1024;

            // errors
            const int E_TNS_AGENT_API_SUCCESS = 0;
            const int E_TNS_AGENT_API_ERROR_BASE = -40000;
            const int E_TNS_AGENT_API_ERROR_EMPTY_NAME = E_TNS_AGENT_API_ERROR_BASE - 1;		//name Ϊ��
            const int E_TNS_AGENT_API_ERROR_EMPTY_VALUE = E_TNS_AGENT_API_ERROR_BASE - 2;		//valueΪ��
            const int E_TNS_AGENT_API_ERROR_ACCESS_DENIED = E_TNS_AGENT_API_ERROR_BASE - 3;		//û�з���Ȩ��
            const int E_TNS_AGENT_API_ERROR_NOT_CONSISTENT = E_TNS_AGENT_API_ERROR_BASE - 4;	//���ݲ�һ��
            const int E_TNS_AGENT_API_ERROR_INIT_FAILED = E_TNS_AGENT_API_ERROR_BASE - 5;		//��ʼ��ʧ��
            const int E_TNS_AGENT_API_ERROR_TNS_UNAVAILABLE = E_TNS_AGENT_API_ERROR_BASE - 6;	//TNS������
            const int E_TNS_AGENT_API_ERROR_PARAMS = E_TNS_AGENT_API_ERROR_BASE - 7;		//��������
            const int E_TNS_AGENT_API_ERROR_PROTOCOL_ERROR = E_TNS_AGENT_API_ERROR_BASE - 8;	//Э�����
            const int E_TNS_AGENT_API_ERROR_NAME_NOT_EXISTS = E_TNS_AGENT_API_ERROR_BASE - 9;	//name������
            const int E_TNS_AGENT_API_ERROR_NAME_TOO_LARGE = E_TNS_AGENT_API_ERROR_BASE - 10;	//name����
            const int E_TNS_AGENT_API_ERROR_VALUE_TOO_LARGE = E_TNS_AGENT_API_ERROR_BASE - 11;	//value����
            const int E_TNS_AGENT_API_ERROR_INVALID_PATH = E_TNS_AGENT_API_ERROR_BASE - 12;		//�Ƿ�·��
            const int E_TNS_AGENT_API_ERROR_NOT_LOCK_OWNER = E_TNS_AGENT_API_ERROR_BASE - 13;	//����ӵ����
            const int E_TNS_AGENT_API_ERROR_NAME_IS_LOCKED = E_TNS_AGENT_API_ERROR_BASE - 14;	//name��������״̬
            const int E_TNS_AGENT_API_ERROR_INVALID_LOCK_SEQ = E_TNS_AGENT_API_ERROR_BASE - 15;	//�Ƿ�lock seq��
            const int E_TNS_AGENT_API_ERROR_LOCK_NOT_EXIST = E_TNS_AGENT_API_ERROR_BASE - 16;	//��������
            const int E_TNS_AGENT_API_ERROR_TIMEOUT = E_TNS_AGENT_API_ERROR_BASE - 17;		//����ʱ
            const int E_TNS_AGENT_API_ERROR_NOT_INIT = E_TNS_AGENT_API_ERROR_BASE - 18;		//Ϊ��ʼ��
            const int E_TNS_AGENT_API_ERROR_INVALID_HANDLE = E_TNS_AGENT_API_ERROR_BASE - 19;	//�Ƿ�handle���
            const int E_TNS_AGENT_API_ERROR_INTR = E_TNS_AGENT_API_ERROR_BASE - 20;			//�쳣�ж�
            const int E_TNS_AGENT_API_ERROR_TASK_EXIST = E_TNS_AGENT_API_ERROR_BASE - 21;		//�����Ѵ���
            const int E_TNS_AGENT_API_ERROR_EVENT_EXIST = E_TNS_AGENT_API_ERROR_BASE - 22;		//�¼��Ѵ���
            const int E_TNS_AGENT_API_EXCEPTION_EVENT = E_TNS_AGENT_API_ERROR_BASE - 23;		//�쳣�¼�֪ͨ
            const int E_TNS_AGENT_API_ERROR_EVENT_TYPE = E_TNS_AGENT_API_ERROR_BASE - 24;		//�����¼�����
            const int E_TNS_AGENT_API_ERROR_OVER_LOAD = E_TNS_AGENT_API_ERROR_BASE - 25;		//�������
            const int E_TNS_AGENT_API_ERROR_TRY_AGAIN = E_TNS_AGENT_API_ERROR_BASE - 26;		//�쳣��������
            const int E_TNS_AGENT_API_ERROR_NAME_CONFLICT = E_TNS_AGENT_API_ERROR_BASE - 27;	//name��ͻ
            const int E_TNS_AGENT_API_ERROR_GET_HOST_INFO_FAILED = E_TNS_AGENT_API_ERROR_BASE - 28; //��ȡ������Ϣʧ��,�޷���ɳ�ʼ��

            // callbacks
            typedef void (*Callback_Set)(int ret, const std::string& errmsg, const std::string& user);
            typedef void (*Callback_Get)(int ret, const std::string& err, const std::string& value, const std::string& user);
            typedef void (*Callback_Del)(int ret, const std::string& err, const std::string& user);
            typedef void (*Callback_Lock)(int ret, const std::string& err, unsigned lock_seq, unsigned lease_time, const std::string& user);
            typedef void (*Callback_UnLock)(int ret, const std::string& err, const std::string& user);
            typedef void (*Callback_ReNew)(int ret, const std::string& err, unsigned lock_seq, unsigned lease_time, const std::string& user);
            typedef void (*Callback_GetLock)(int ret, const std::string& err, int lock_type, unsigned lease_time, const std::string& user);
            typedef void (*Callback_RegisterDataChangedEvent)(int ret, const std::string& err, const std::string& user);
            typedef void (*Callback_RegisterDirChangedEvent)(int ret, const std::string& err, const std::string& user);
            typedef void (*Callback_RegisterNodeDeletedEvent)(int ret, const std::string& err, const std::string& user);
            typedef void (*Callback_UnregisterDataChangedEvent)(int ret, const std::string& err, const std::string& user);
            typedef void (*Callback_UnregisterDirChangedEvent)(int ret, const std::string& err, const std::string& user);
            typedef void (*Callback_UnregisterNodeDeletedEvent)(int ret, const std::string& err, const std::string& user);
            typedef void (*Callback_ListDir)(int ret, const std::string& err, const std::set<std::string>& dirs, const std::string& user);
            typedef void (*Callback_DataChangedEvent)(const std::string& name);
            typedef void (*Callback_DirChangedEvent)(const std::string& dir, const std::string& new_path);
            typedef void (*Callback_NodeDeletedEvent)(const std::string& name);
            typedef void (*Callback_LockTimeoutEvent)(const std::string& name);
            typedef void (*Callback_AddStat)(int ret, const std::string& retmsg, const std::string& user);

            class CTnsCallbacks
            {
            public:
                CTnsCallbacks()
                {
                    set_callback = NULL;
                    get_callback = NULL;
                    del_callback = NULL;
                    lock_callback = NULL;
                    unlock_callback = NULL;
                    renew_callback = NULL;
                    get_lock_callback = NULL;
                    register_data_changed_event_callback = NULL;
                    register_dir_changed_event_callback = NULL;
                    register_node_deleted_event_callback = NULL;
                    unregister_data_changed_event_callback = NULL;
                    unregister_dir_changed_event_callback = NULL;
                    unregister_node_deleted_event_callback = NULL;
                    listdir_callback = NULL;
                    data_changed_event_callback = NULL;
                    dir_changed_event_callback = NULL;
                    node_deleted_event_callback = NULL;
                    lock_timeout_event_callback = NULL;
                    add_stat_callback = NULL;
                }
        
                Callback_Set set_callback;
                Callback_Get get_callback;
                Callback_Del del_callback;
                Callback_Lock lock_callback;
                Callback_UnLock unlock_callback;
                Callback_ReNew renew_callback;
                Callback_GetLock get_lock_callback;
                Callback_RegisterDataChangedEvent register_data_changed_event_callback;
                Callback_RegisterDirChangedEvent register_dir_changed_event_callback;
                Callback_RegisterNodeDeletedEvent register_node_deleted_event_callback;
                Callback_UnregisterDataChangedEvent unregister_data_changed_event_callback;
                Callback_UnregisterDirChangedEvent unregister_dir_changed_event_callback;
                Callback_UnregisterNodeDeletedEvent unregister_node_deleted_event_callback;
                Callback_ListDir listdir_callback;
                Callback_DataChangedEvent data_changed_event_callback;
                Callback_DirChangedEvent dir_changed_event_callback;
                Callback_NodeDeletedEvent node_deleted_event_callback;
                Callback_LockTimeoutEvent lock_timeout_event_callback;
                Callback_AddStat add_stat_callback;
            };

            typedef enum
            {
                LockType_Shared,
                LockType_Exclusive
            } LockType;

            typedef enum
            {
                EventType_LockTimeout = 0,
                EventType_DataChange,
                EventType_DirChange,
                EventType_NodeDeleted,
                EventType_Unknown
            } EventType;

            // api class
            class CTnsAgentApi
            {
            public:
                CTnsAgentApi()
                {
                    _socket_fd = -1;
                    _connected = false;
                    _buf = new char[BUF_SIZE];
                }

                ~CTnsAgentApi()
                {
                    if (_buf)
                    {
                        delete [] _buf;
                    }
                }

                /***************************************************
                 * ��ʼ��TNS Agent api
                 *
                 * ������
                 * config_file - TNS Agent Api�����ļ�·��
                 *
                 * ����ֵ
                 * 0 - �ɹ�
                 * <0 - ʧ��
                 ****************************************************/
                int init(const std::string& config_file);

                /***************************************************
                 * �첽Set�ӿ�
                 *
                 * ������
                 * name - ���������
                 * value - ��Ҫset��value
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int set(const std::string& name, const std::string& value, const std::string& user);

                /***************************************************
                 * �첽Set_Tmp�ӿ�
                 *
                 * ������
                 * name - ���������
                 * value - ��Ҫset��value
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int set_tmp(const std::string& name, const std::string& value, const std::string& user);

                /***************************************************
                 * �첽Get�ӿ�
                 *
                 * ������
                 * name - ���������
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int get(const std::string& name, const std::string& user);

                /***************************************************
                 * �첽Del�ӿ�
                 *
                 * ������
                 * name - ���������
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int del(const std::string& name, const std::string& user);

                /***************************************************
                 * �첽Lock�ӿ�
                 *
                 * ������
                 * name - ������
                 * type - ������
                 * lease_time - ����Լʱ��
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int lock(const std::string& name, LockType type, unsigned lease_time, const std::string& user);

                /***************************************************
                 * �첽������ӿ�
                 *
                 * ������
                 * name - ������
                 * lock_seq - �����к�
                 * type - ������
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int renew_lock(const std::string& name, unsigned lock_seq, LockType type, unsigned lease_time, const std::string& user);

                /***************************************************
                 * �첽����Ϣ��ȡ�ӿ�
                 *
                 * ������
                 * name - ������
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int get_lock(const std::string& name, const std::string& user);

                /***************************************************
                 * �첽�����ӿ�
                 *
                 * ������
                 * name - ������
                 * lock_seq - �����к�
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int unlock(const std::string& name, unsigned lock_seq, const std::string& user);

                /***************************************************
                 * �첽���ݸı��¼�ע��ӿ�
                 *
                 * ������
                 * name - ����
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int register_data_changed_event(const std::string& name, const std::string& user);

                /***************************************************
                 * �첽���ݸı��¼�ע���ӿ�
                 *
                 * ������
                 * name - ����
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int unregister_data_changed_event(const std::string& name, const std::string& user);

                /***************************************************
                 * �첽Ŀ¼�ı��¼�ע��ӿ�
                 *
                 * ������
                 * name - ����
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int register_dir_changed_event(const std::string& name, const std::string& user);

                /***************************************************
                 * �첽Ŀ¼�ı��¼�ע���ӿ�
                 *
                 * ������
                 * name - ����
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int unregister_dir_changed_event(const std::string& name, const std::string& user);

                /***************************************************
                 * �첽�ڵ�ɾ���¼�ע��ӿ�
                 *
                 * ������
                 * name - ����
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int register_node_deleted_event(const std::string& name, const std::string& user);

                /***************************************************
                 * �첽�ڵ�ɾ���¼�ע���ӿ�
                 *
                 * ������
                 * name - ����
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int unregister_node_deleted_event(const std::string& name, const std::string& user);

                /***************************************************
                 * �첽ls�ӿ�
                 *
                 * ������
                 * path - ·��
                 * user - �û���������ݣ����ڻص��з���
                 *
                 * ����ֵ
                 * 0 - �ɹ������첽����
                 * <0 - ʧ��
                 ****************************************************/
                int listdir(const std::string& path, const std::string& user);

                /***************************************************
                 * �ۼ�ͳ��ֵ�ӿ�
                 *
                 * ������
                 * stat_name - ͳ��ֵ����
                 * begin - ͳ�ƿ�ʼʱ��
                 * end - ͳ�ƽ���ʱ��
                 *
                 * ����ֵ
                 * 0 - �ɹ�
                 * <0 - ʧ��
                 ****************************************************/
                int add_stat(const std::string& stat_name, const struct timeval* begin, const struct timeval* end, const std::string& user);

                /***************************************************
                 * ���ûص�����
                 *
                 * ������
                 * callbacks - �ص���������
                 *
                 * ����ֵ
                 * ��
                 ****************************************************/
                void set_callbacks(const CTnsCallbacks callbacks)
                {
                    _callbacks = callbacks;
                }

                /***************************************************
                 * ��ȡ�ص�����
                 *
                 * ������
                 * callbacks - �ص���������
                 *
                 * ����ֵ
                 * ��
                 ****************************************************/
                void get_callbacks(CTnsCallbacks& callbacks)
                {
                    callbacks = _callbacks;
                }

                // ִ�з������շ�������
                void dispatch();

            protected:
                int do_connect();
                int do_reconnect();
                int do_socket_create();
                void dispatch_input();
                int do_write(storage::tns::asn::StorageMessage& msg);
                int send_buffered_data();

                // rsp handlers
                void on_set_rsp(storage::tns::asn::StorageMessage& msg);
                void on_get_rsp(storage::tns::asn::StorageMessage& msg);
                void on_del_rsp(storage::tns::asn::StorageMessage& msg);
                void on_list_dir_rsp(storage::tns::asn::StorageMessage& msg);
                void on_register_event_rsp(storage::tns::asn::StorageMessage& msg);
                void on_unregister_event_rsp(storage::tns::asn::StorageMessage& msg);
                void on_notify_event_req(storage::tns::asn::StorageMessage& msg);
                void on_lock_name_rsp(storage::tns::asn::StorageMessage& msg);
                void on_unlock_name_rsp(storage::tns::asn::StorageMessage& msg);
                void on_renew_lock_lease_rsp(storage::tns::asn::StorageMessage& msg);
                void on_get_name_lock_rsp(storage::tns::asn::StorageMessage& msg);
                void on_add_stat_rsp(storage::tns::asn::StorageMessage& msg);

            public:                
                CTnsCallbacks _callbacks;

            private:
                std::string _config_file;
                std::string _bind_path;
                unsigned _send_buf_size;
                storage::tns::nagent::CRingBuffer* _ring_buf;
                int _socket_fd;
                bool _connected;
                char* _buf;
             };
        }
    }
}

#endif
