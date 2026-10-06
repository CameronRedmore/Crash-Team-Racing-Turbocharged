import sys
from PIL import Image
import numpy as np
im = np.array(Image.open(sys.argv[1]).convert("RGB")).astype(int)
box = im[60:620, 760:1160]
cyan = (box[:, :, 0] < 170) & (box[:, :, 1] > 190) & (box[:, :, 2] > 170) & (box[:, :, 1] - box[:, :, 0] > 50)
# The main menu's border is a long vertical cyan line on both sides.
cols = cyan.sum(axis=0)
sys.exit(0 if (cols > 300).sum() >= 2 else 1)
