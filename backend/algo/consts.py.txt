from enum import Enum


class Direction(int, Enum):
    NORTH = 0
    EAST = 2
    SOUTH = 4
    WEST = 6
    SKIP = 8

    def __int__(self):
        return self.value

    @staticmethod
    def rotation_cost(d1, d2):
        diff = abs(d1 - d2)
        return min(diff, 8 - diff)

MOVE_DIRECTION = [
    (1, 0, Direction.EAST),
    (-1, 0, Direction.WEST),
    (0, 1, Direction.NORTH),
    (0, -1, Direction.SOUTH),
]

# Physical arena dimensions (in centimeters)
ARENA_WIDTH_CM = 200
ARENA_HEIGHT_CM = 200

# Grid resolution (number of cells)
GRID_CELLS_X = 20
GRID_CELLS_Y = 20

# Cell size in centimeters
CELL_SIZE_CM = ARENA_WIDTH_CM / GRID_CELLS_X  # 10 cm per cell

# Robot physical parameters (in centimeters)
#change according to STM TONY
TURN_RADIUS_CM = 1  # Actual physical turning radius of the robot

# Algorithm parameters (in grid cells)
WIDTH = GRID_CELLS_X  # 20 cells
HEIGHT = GRID_CELLS_Y  # 20 cells
TURN_RADIUS = round(TURN_RADIUS_CM / CELL_SIZE_CM)  # Convert 19cm to cells: 1.9 ≈ 2 cells
EXPANDED_CELL = 1  # for both agent and obstacles

# Algorithm tuning parameters
TURN_FACTOR = 1
ITERATIONS = 2000
SAFE_COST = 1000  # the cost for the turn in case there is a chance that the robot is touch some obstacle
SCREENSHOT_COST = 50  # the cost for the place where the picture is taken