# trackteur
Asset tracking documentation

python3 -m venv myenv

source myenv/bin/activate

pip install -r requirements.txt

python3 get_sx1276_version.py
should get SX1276 Version: 0x12 (if SX1276 is plugged)

python3 get_gps, should get gps position

Receiver
python3 rx.py

Transmitter
python3 test.py (for sending a test packet)
python3 tx_gps.py (send gps position)
