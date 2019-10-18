class Sqlite {
    constructor(file) {
        if(!native.open(this, file)) {
            throw "open failed";
        }
    }

    execute(sql, callback) {
        native.execute(this, sql, callback);
    }
}

module.exports = Sqlite;
module.exports.Sqlite = Sqlite;
