
app.on('sessionOpened', (data, client)=>{

    console.log('(INFO) app.on(sessionOpened)')

    send('localhost', 18080, '/client_connected')

})

