
const fs = require('fs');
const gulp = require('gulp');
const htmlmin = require('gulp-htmlmin');
const cleancss = require('gulp-clean-css');
const uglify = require('gulp-uglify');
const gzip = require('gulp-gzip');
const del = require('del');
const inline = require('gulp-inline');
const inlineImages = require('gulp-css-base64');
const favicon = require('gulp-base64-favicon');


const dataFolder = 'static/';


function clean(done) 
{
    del([ dataFolder + '*']);
    done();
}


function buildfs_embeded(done) 
{
    var files = ["index.html.gz", "wifi.html.gz", "opcje.html.gz","admin.html.gz","graph.html.gz","kolumna.html.gz","update.html.gz","helpwifi.html.gz"];

    var n = 0;
    for(n = 0; n < files.length ;n++)
    {
    var wstream = fs.createWriteStream(dataFolder + files[n] + '.h');
    wstream.on('error', function (err)
    {
        console.log(err);
    });

    var data = fs.readFileSync(dataFolder + files[n]);
    
		var name_array =  files[n].split(".");
		var name_new = "";
		var ns = 0;
		for(ns = 0; ns < name_array.length ; ns++)
		{
			name_new += name_array[ns] + '_';
		}
        
        name_new = name_new.substr(0, name_new.length - 1);
        
        wstream.write('#define ' + name_new + '_len ' + data.length + '\n');
        wstream.write('const uint8_t ' + name_new + '[] PROGMEM = {')

    for (i=0; i<data.length; i++)
    {
        if (i % 1000 == 0) wstream.write("\n");
        wstream.write('0x' + ('00' + data[i].toString(16)).slice(-2));
        if (i<data.length-1) wstream.write(',');
    }
    wstream.write('\n};')
    wstream.end();
    del([ dataFolder + files[n] ]);
   }
   
   done();
}

function buildfs_inline() 
{
    return gulp.src('html/*.html')
        
       // .pipe(favicon())
        
        .pipe(inline(
        {
            base: 'html/',
            js: uglify,
            css: [cleancss],
            disabledTypes: ['svg', 'img']
          }))
      
        .pipe(htmlmin({
            collapseWhitespace: true,
            removecomments: true,
			aside: true,
            minifyCSS: true,
            minifyJS: true
        }))
      
        .pipe(gzip())
        .pipe(gulp.dest(dataFolder));
}


gulp.task('default', gulp.series(clean,buildfs_inline, buildfs_embeded));




