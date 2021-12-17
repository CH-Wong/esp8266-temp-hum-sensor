d3.csv("data.csv")
.then(makeChart);

function makeChart(data) {

    var options = { year: 'numeric', month: 'short', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric' };
    var time = data.map(function(d) {return new Date(parseInt(d.Time)*1000).toLocaleString("en-GB", options)})
    var humidity = data.map(function(d) {return d.Humidity})
    var temperature = data.map(function(d) {return d.Temperature})

    var tempColor = '#b32400'
    var humColor = '#002db3'

    var dataChart = new Chart(document.getElementById('dataChart'), {
        type: 'line',
        data: {
            labels: time,
            datasets: [
                {
                    label: 'Temperature [C]',
                    data:  temperature,
                    yAxisID: 'T',
                    radius: 0,
                    borderWidth: 1,
                    borderColor: tempColor,
                    backgroundColor: tempColor,
                },
                {
                    label: 'Humidity [%]',
                    data: humidity,
                    yAxisID: 'H',
                    radius: 0,
                    borderWidth: 1,
                    borderColor: humColor,
                    backgroundColor: humColor,
                }
            ]
        },
        options: {
            responsive: true,
            interaction: {
                mode: 'index',
                intersect: false,
              },
            plugins: {
                title: {
                    display: true,
                    text: 'Measurement Station #1: Living Room'
                },
                legend: {
                    position: 'top'
                },
                zoom: {
                    zoom: {
                        wheel: {
                          enabled: true,
                        },
                        pinch: {
                          enabled: true,
                        },
                        mode: 'x'
                      },
                      pan: {
                        enabled: true,
                        mode: 'xy'
                      }
                }
              },
            scales: {
                x: {
                    title: {
                        display: true,
                        text: 'Measurement Date + Time'
                      },
                },
                T: {
                    type: 'linear',
                    position: 'left',
                    min: 0,
                    max: 50,
                    title: {
                        display: true,
                        text: 'Temperature [C]',
                        color: tempColor
                    },
                    ticks: {
                            color: tempColor,
                            fontSize: 18,
                            padding: 10
                        }
                },
                H: {
                    type: 'linear',
                    position: 'right',
                    min: 0,
                    max: 100,
                    title: {
                        display: true,
                        text: 'Humidity [%]',
                        color: humColor
                    },
                    ticks: {
                            color: humColor,
                            fontSize: 18,
                            padding: 10
                        }
                }
            }
        }
    });

}

