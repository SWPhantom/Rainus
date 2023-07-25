
```console
# Set up python env:
pip install virtualenv
virtualenv venv
source venv/bin/activate
pip install -r requirements.txt

# To run the program:
python process.py [-r|--regression] -s|--source path/to/csv/file.txt

# Example:
python process.py -s ../logs/rainLogOvernight.txt -r
```
This will graph the temperature and humidity in one graph
and the time separation between consecutive data points in another graph

The --regression flag is meant to be run when the drip rate is supposed to be consistent/automatic. This will show if there is variation in a rate of activation. 