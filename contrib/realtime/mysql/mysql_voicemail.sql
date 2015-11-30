CREATE TABLE alembic_version (
    version_num VARCHAR(32) NOT NULL
);

-- Running upgrade None -> a2e9769475e

CREATE TABLE voicemail_messages (
    dir VARCHAR(255) NOT NULL, 
    msgnum INTEGER NOT NULL, 
    context VARCHAR(80), 
    macrocontext VARCHAR(80), 
    callerid VARCHAR(80), 
    origtime INTEGER, 
    duration INTEGER, 
    recording BLOB, 
    flag VARCHAR(30), 
    category VARCHAR(30), 
    mailboxuser VARCHAR(30), 
    mailboxcontext VARCHAR(30), 
    msg_id VARCHAR(40)
);

ALTER TABLE voicemail_messages ADD CONSTRAINT voicemail_messages_dir_msgnum PRIMARY KEY (dir, msgnum);

CREATE INDEX voicemail_messages_dir ON voicemail_messages (dir);

INSERT INTO alembic_version (version_num) VALUES ('a2e9769475e');

-- Running upgrade a2e9769475e -> 39428242f7f5

ALTER TABLE voicemail_messages CHANGE recording recording BLOB(4294967295) NULL;

UPDATE alembic_version SET version_num='39428242f7f5';

