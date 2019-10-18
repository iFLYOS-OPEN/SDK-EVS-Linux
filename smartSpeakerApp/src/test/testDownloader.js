var BatchDownloader = require('../batchDownloader').BatchDownler;
var console = require('console');


var downloader = new BatchDownloader([
    {
        url:'https://cdn.iflyos.cn/public/adam/76ed1b65-cb52-4e5e-ab06-078efae9c8d5/wakeword2.jet',
        name: 'caeRes'
    },
    {
        url: 'https://cdn.iflyos.cn/public/adam/76ed1b65-cb52-4e5e-ab06-078efae9c8d5/ready_voice.mp3',
        name: 'hi'
    },
    {
        url: 'https://cdn.iflyos.cn/public/adam/76ed1b65-cb52-4e5e-ab06-078efae9c8d5/remove_voice.mp3',
        name: 'bye'
    }
], '/tmp');

downloader.start();

//setInterval(()=>{}, 10000);