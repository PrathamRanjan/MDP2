#!/usr/bin/env python3
"""
Main Communication Handler for MDP Robot System
Handles the complete communication flow:
Android (Bluetooth) -> RPI -> Algorithm Backend (Laptop) -> RPI -> STM32

Also integrates vision system for letter detection:
Pi Camera -> Model Server (Laptop) -> Android (via Bluetooth)

Based on working STM32 communication code provided by user.
"""

import serial
import time
import json
import requests
import threading
import logging
from typing import Dict, List, Optional
import configparser
import os

# Import vision client
from vision_client import VisionClient

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class MDPCommunicationHandler:
    def __init__(self, config_file='config.ini'):
        """Initialize the communication handler with configuration"""
        self.config = configparser.ConfigParser()
        config_path = os.path.join(os.path.dirname(__file__), config_file)
        self.config.read(config_path)
        
        # STM32 Serial Connection (based on working code)
        try:
            self.stm32_port = self.config.get('stm32', 'port')
        except:
            self.stm32_port = '/dev/serial/by-id/usb-1a86_USB_Single_Serial_5A6C067446-if00'
        
        try:
            self.stm32_baudrate = self.config.getint('stm32', 'baudrate')
        except:
            self.stm32_baudrate = 115200
        
        try:
            self.stm32_timeout = self.config.getint('stm32', 'timeout')
        except:
            self.stm32_timeout = 1
            
        self.stm32_serial = None
        
        # Algorithm Backend Connection (from main.py)
        try:
            self.algo_host = self.config.get('algorithm', 'host')
        except:
            self.algo_host = '192.168.12.31'
        
        try:
            self.algo_port = self.config.getint('algorithm', 'port')
        except:
            self.algo_port = 5001
            
        self.algo_url = "http://{}:{}".format(self.algo_host, self.algo_port)
        
        # Vision Server Connection (for letter detection)
        try:
            self.vision_host = self.config.get('vision', 'host')
        except:
            self.vision_host = '192.168.12.31'
        
        try:
            self.vision_port = self.config.getint('vision', 'port')
        except:
            self.vision_port = 8000
        
        # Bluetooth RFCOMM connection
        self.bluetooth_device = '/dev/rfcomm0'
        self.bluetooth_conn = None
        
        # Vision system
        self.client = None
        self.vision_enabled = False
        
        # State management
        self.running = False
        self.mission_active = False
        
    def initialize(self):
        """Initialize all connections"""
        logger.info("Initializing MDP Communication Handler...")
        
        # Initialize STM32 connection
        self._init_stm32()
        
        # Test algorithm backend connection
        self._test_algo_connection()
        
        # Initialize vision system
        self._init_vision_system()
        
        # Note: Vision detection will be started in a separate thread after Bluetooth starts
        # to prevent blocking the main communication handler
        
        logger.info("Communication handler initialized successfully")
    
    def _init_stm32(self):
        """Initialize STM32 serial connection (based on working code)"""
        try:
            logger.info("Connecting to STM32 on {}".format(self.stm32_port))
            self.stm32_serial = serial.Serial(
                self.stm32_port, 
                self.stm32_baudrate, 
                timeout=self.stm32_timeout
            )
            time.sleep(2)  # Wait for STM32 to be ready
            logger.info("STM32 connection established")
        except Exception as e:
            logger.error("Failed to connect to STM32: {}".format(e))
            raise
    
    def _test_algo_connection(self):
        """Test connection to algorithm backend"""
        try:
            response = requests.get("{}/status".format(self.algo_url), timeout=5)
            if response.status_code == 200:
                logger.info("Algorithm backend connection successful")
            else:
                logger.warning("Algorithm backend responded with status {}".format(response.status_code))
        except Exception as e:
            logger.error("Failed to connect to algorithm backend: {}".format(e))
            logger.warning("Continuing without algorithm backend connection")
    
    def _init_vision_system(self):
        """Initialize vision system for letter detection"""
        try:
            logger.info("Initializing vision system...")
            self.client = VisionClient(
                server_ip=self.vision_host,
                server_port=self.vision_port,
                callback=self._on_letter_detected
            )
            
            # Vision client is available (connection will be tested when detection starts)
            self.vision_enabled = True
            logger.info("Vision system available")
                
        except Exception as e:
            logger.error("Failed to initialize vision system: {}".format(e))
            logger.warning("Continuing without vision system")
    
    def _on_letter_detected(self, letter: str):
        """Callback when a letter is detected by vision system"""
        logger.info("Letter detected by vision: {}".format(letter))
        
        # Send detected letter to Android in the exact format it expects for STATUS window
        if self.bluetooth_conn:
            # Android expects format: "STATUS,message" - this will show in status window
            status_message = "STATUS,Letter detected: {}\n".format(letter)
            try:
                self.bluetooth_conn.write(status_message.encode('utf-8'))
                self.bluetooth_conn.flush()
                logger.info("Sent detected letter '{}' to Android as STATUS message: {}".format(letter, repr(status_message)))
            except Exception as e:
                logger.error("Error sending letter to Android: {}".format(e))
        else:
            logger.warning("No Bluetooth connection available to send detected letter")
    
    def start_vision_detection(self):
        """Start continuous letter detection in a separate thread"""
        if self.vision_enabled and self.client:
            try:
                # Start vision detection in a separate thread to avoid blocking Bluetooth
                vision_thread = threading.Thread(
                    target=self._run_vision_detection,
                    daemon=True,
                    name="VisionDetectionThread"
                )
                vision_thread.start()
                logger.info("Vision detection thread started")
                return True
            except Exception as e:
                logger.error("Error starting vision detection thread: {}".format(e))
                return False
        else:
            logger.warning("Vision system not available")
            return False
    
    def _run_vision_detection(self):
        """Run vision detection in a separate thread"""
        try:
            logger.info("Starting vision detection in background thread...")
            if self.client.start_continuous_detection():
                logger.info("Vision detection running successfully")
            else:
                logger.error("Vision detection failed to start")
        except Exception as e:
            logger.error("Error in vision detection thread: {}".format(e))
    
    def stop_vision_detection(self):
        """Stop letter detection"""
        if self.client:
            self.client.stop_detection()
            logger.info("Vision detection stopped")
    
    def start_bluetooth_listener(self):
        """Start listening for Bluetooth connections from Android"""
        logger.info("Starting Bluetooth listener...")
        logger.info("NOTE: Make sure 'sudo rfcomm listen /dev/rfcomm0 1' is running in another terminal")
        
        # NOTE: We no longer auto-start continuous vision detection since we use on-demand detection
        if self.vision_enabled:
            logger.info("Vision system is available for on-demand detection")
        else:
            logger.warning("Vision system not available")
        
        self.running = True
        
        while self.running:
            try:
                # Wait for the RFCOMM device to be available
                logger.info("Waiting for RFCOMM connection...")
                
                # Check if rfcomm0 exists and is ready
                import os
                import time
                
                # Wait for the device to become available
                while not os.path.exists(self.bluetooth_device):
                    logger.info("Waiting for {} to become available...".format(self.bluetooth_device))
                    time.sleep(2)
                
                # Try to open the device with proper mode
                logger.info("Attempting to open {}...".format(self.bluetooth_device))
                
                # Open in binary read/write mode
                self.bluetooth_conn = open(self.bluetooth_device, 'rb+', buffering=0)
                logger.info("Successfully opened Bluetooth connection")
                
                # Handle communication with Android
                self._handle_android_communication()
                
            except PermissionError:
                logger.error("Permission denied accessing {}. Make sure you run with sudo or add user to dialout group".format(self.bluetooth_device))
                time.sleep(5)
            except FileNotFoundError:
                logger.error("Bluetooth device {} not found.".format(self.bluetooth_device))
                logger.info("Make sure 'sudo rfcomm listen /dev/rfcomm0 1' is running")
                time.sleep(5)
            except Exception as e:
                logger.error("Bluetooth connection error: {}".format(e))
                time.sleep(2)
            finally:
                if self.bluetooth_conn:
                    try:
                        self.bluetooth_conn.close()
                    except:
                        pass
                    self.bluetooth_conn = None
    
    def _handle_android_communication(self):
        """Handle communication with Android device"""
        logger.info("Ready to receive data from Android...")
        
        while self.running and self.bluetooth_conn:
            try:
                # Read data from the Bluetooth device
                logger.debug("Checking for incoming data...")
                
                # Read data with timeout
                data = self.bluetooth_conn.read(1024)
                
                if data:
                    logger.info("Raw data received: {}".format(repr(data)))
                    
                    try:
                        message = data.decode('utf-8').strip()
                        if message:  # Only process non-empty messages
                            logger.info("Decoded message: '{}'".format(message))
                            
                            # Process the message
                            self._process_android_message(message)
                        else:
                            logger.debug("Received empty message after decoding")
                    except UnicodeDecodeError as e:
                        logger.error("Failed to decode data: {} - Raw: {}".format(e, repr(data)))
                else:
                    # No data received, short sleep to prevent busy waiting
                    time.sleep(0.1)
                
            except Exception as e:
                logger.error("Error in Android communication: {}".format(e))
                break
    
    def _process_android_message(self, message: str):
        """Process message received from Android"""
        try:
            logger.info("Processing message: '{}'".format(message))
            
            # Check if this is the ALG: format from Android
            if message.startswith("ALG:"):
                logger.info("Received ALG obstacle data from Android")
                self._handle_alg_obstacle_data(message)
            else:
                # Try to parse as JSON for other message types
                try:
                    data = json.loads(message)
                    logger.info("Parsed JSON data: {}".format(data))
                    
                    if data.get('type') == 'mission_start':
                        logger.info("Mission start command received")
                        self._handle_mission_start(data)
                    elif data.get('type') == 'emergency_stop':
                        logger.info("Emergency stop received")
                        self._handle_emergency_stop()
                    elif data.get('type') == 'start_vision':
                        logger.info("Start vision command received")
                        self._handle_start_vision()
                    elif data.get('type') == 'stop_vision':
                        logger.info("Stop vision command received")
                        self._handle_stop_vision()
                    else:
                        logger.warning("Unknown message type: {}".format(data.get('type')))
                        
                except json.JSONDecodeError:
                    logger.error("Invalid format received: '{}' - Expected ALG: format or JSON".format(message))
                    
        except Exception as e:
            logger.error("Error processing Android message: {}".format(e))
    
    def _handle_alg_obstacle_data(self, message: str):
        """Handle ALG: format obstacle data from Android"""
        try:
            # Remove "ALG:" prefix
            obstacle_data = message[4:]
            logger.info("Processing obstacle data: '{}'".format(obstacle_data))
            
            # Parse obstacles from format: x,y,direction,id;x,y,direction,id;...
            # BUT Android actually sends: x,y,direction,0;x,y,direction,1;... (with trailing comma before ID)
            obstacles = []
            obstacle_entries = obstacle_data.split(';')
            
            for entry in obstacle_entries:
                if entry.strip():  # Skip empty entries
                    parts = entry.split(',')
                    logger.info("Parsing entry: '{}' -> parts: {}".format(entry, parts))
                    
                    # Handle the actual Android format: "x,y,direction,id" or "x,y,direction,id"
                    if len(parts) >= 4:
                        try:
                            x = int(parts[0])
                            y = int(parts[1])
                            direction_str = parts[2]
                            # The ID might be in parts[3] or parts[4] depending on trailing comma
                            obstacle_id = None
                            for i in range(3, len(parts)):
                                if parts[i].isdigit():
                                    obstacle_id = int(parts[i])
                                    break
                            
                            if obstacle_id is None:
                                logger.warning("Could not find valid ID in entry: {}".format(entry))
                                continue
                            
                            # Convert direction string to number (0=North, 1=East, 2=South, 3=West)
                            direction_map = {'N': 0, 'E': 2, 'S': 4, 'W': 6}
                            direction = direction_map.get(direction_str, 0)
                            
                            obstacles.append({
                                'x': x,
                                'y': y,
                                'd': direction,
                                'id': obstacle_id
                            })
                            logger.info("Added obstacle: x={}, y={}, d={}, id={}".format(x, y, direction, obstacle_id))
                        except ValueError as e:
                            logger.error("Error parsing obstacle entry '{}': {}".format(entry, e))
                    else:
                        logger.warning("Invalid obstacle entry format: '{}' (expected at least 4 parts)".format(entry))
            
            logger.info("Parsed {} obstacles total".format(len(obstacles)))
            
            # Debug: Log each obstacle in detail for comparison with simulator
            for obs in obstacles:
                logger.info("DEBUG: Obstacle ID {} -> x={}, y={}, direction={} (Android->Algorithm format)".format(
                    obs['id'], obs['x'], obs['y'], obs['d']))
            
            # TODO: Get actual robot position from Android instead of hardcoding
            # For now, using default values - THIS COULD BE THE ISSUE!
            robot_x = 1  # Should come from Android
            robot_y = 1  # Should come from Android  
            robot_dir = 0  # Should come from Android
            
            logger.info("DEBUG: Robot position for algorithm -> x={}, y={}, direction={}".format(robot_x, robot_y, robot_dir))
            
            # Create payload for algorithm backend
            payload = {
                'obstacles': obstacles,
                'robot_x': robot_x,
                'robot_y': robot_y,
                'robot_dir': robot_dir,
                'retrying': False
            }
            
            logger.info("DEBUG: Full payload being sent to algorithm: {}".format(payload))
            self._send_to_algorithm_backend(payload)
            
        except Exception as e:
            logger.error("Error handling ALG obstacle data: {}".format(e))
    
    def _send_to_algorithm_backend(self, payload):
        """Send payload to algorithm backend"""
        try:
            # Send to algorithm backend
            response = requests.post(
                "{}/path".format(self.algo_url),
                json=payload,
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                commands = result['data']['commands']
                low_level_commands = result['data'].get('low_level_commands', [])
                
                logger.info("Received {} high-level commands from algorithm".format(len(commands)))
                logger.info("Received {} low-level commands from algorithm".format(len(low_level_commands)))
                
                # Send acknowledgment to Android (if connected)
                if self.bluetooth_conn:
                    self._send_to_android({'type': 'path_received', 'command_count': len(low_level_commands)})
                
                # Execute low-level commands directly on STM32
                if low_level_commands:
                    self._execute_low_level_commands(low_level_commands)
                else:
                    logger.warning("No low-level commands received from algorithm - skipping execution")
                    if self.bluetooth_conn:
                        self._send_to_android({'type': 'error', 'message': 'No executable commands received from algorithm'})
                
            else:
                logger.error("Algorithm backend error: {}".format(response.status_code))
                if self.bluetooth_conn:
                    self._send_to_android({'type': 'error', 'message': 'Path finding failed'})
                    
        except Exception as e:
            logger.error("Error sending to algorithm backend: {}".format(e))
            if self.bluetooth_conn:
                self._send_to_android({'type': 'error', 'message': str(e)})
    
    def _execute_low_level_commands(self, low_level_commands: List[str]):
        """Execute low-level commands in chunks separated by 'M' markers"""
        logger.info("Starting chunked command execution on STM32...")
        
        # First, send a test command to verify STM32 is responding
        logger.info("Testing STM32 connection with simple command...")
        try:
            test_cmd = "T50|0|30\n"
            self.stm32_serial.write(test_cmd.encode())
            logger.info("Test command sent: {}".format(repr(test_cmd)))
            time.sleep(1)
            
            # Check for any response
            if self.stm32_serial.in_waiting:
                response = self.stm32_serial.readline().decode('utf-8').strip()
                logger.info("STM32 test response: {}".format(response))
            else:
                logger.warning("No response from STM32 to test command")
        except Exception as e:
            logger.error("Error in STM32 test: {}".format(e))
        
        # Split commands into chunks by 'M' markers
        command_chunks = self._split_commands_by_markers(low_level_commands)
        logger.info("Split {} commands into {} chunks".format(len(low_level_commands), len(command_chunks)))
        
        total_executed = 0
        
        for chunk_index, chunk in enumerate(command_chunks):
            logger.info("Executing chunk {}/{} with {} commands".format(chunk_index + 1, len(command_chunks), len(chunk)))
            
            # Execute all commands in this chunk
            for i, cmd in enumerate(chunk):
                try:
                    # Add newline if not present and send command to STM32
                    if not cmd.endswith('\n'):
                        cmd = cmd + '\n'
                    
                    # Log what we're actually sending (including bytes)
                    logger.info("[Chunk {}/{}][{}/{}] Sending to STM32: {} (bytes: {})".format(
                        chunk_index + 1, len(command_chunks), i+1, len(chunk), repr(cmd), repr(cmd.encode())))
                        
                    self.stm32_serial.write(cmd.encode())
                    self.stm32_serial.flush()  # Force send immediately
                    
                    total_executed += 1
                    
                    # Update Android with progress
                    if self.bluetooth_conn:
                        progress = {'type': 'progress', 'current': total_executed, 'total': len(low_level_commands)}
                        self._send_to_android(progress)
                    
                    time.sleep(0.1)  # Based on working code timing
                    
                except Exception as e:
                    logger.error("Error sending command to STM32: {}".format(e))
                    if self.bluetooth_conn:
                        self._send_to_android({'type': 'error', 'message': 'STM32 communication error: {}'.format(str(e))})
                    return
            
            # Wait for STM32 responses for this chunk
            logger.info("Waiting for STM32 responses for chunk {}...".format(chunk_index + 1))
            time.sleep(1)  # Shorter wait between chunks
            self._read_stm32_responses()
            
            # After completing a chunk (except the last one), do vision detection
            if chunk_index < len(command_chunks) - 1:  # Not the last chunk
                logger.info("Chunk {} completed. Starting vision detection phase...".format(chunk_index + 1))
                
                # 5-second wait before vision detection
                logger.info("Waiting 5 seconds before image capture...")
                if self.bluetooth_conn:
                    self._send_to_android({'type': 'vision_wait_start'})
                time.sleep(5)
                
                # Capture and process images
                self._perform_vision_detection_sequence()
            else:
                logger.info("Final chunk {} completed".format(chunk_index + 1))
        
        # Notify completion
        if self.bluetooth_conn:
            self._send_to_android({'type': 'mission_complete'})
        logger.info("Mission execution completed")
    
    def _split_commands_by_markers(self, commands: List[str]) -> List[List[str]]:
        """Split command list into chunks separated by 'M' markers"""
        chunks = []
        current_chunk = []
        
        for cmd in commands:
            # Remove newlines and whitespace for comparison
            clean_cmd = cmd.strip()
            
            if clean_cmd == 'M':
                # Found a marker - end current chunk (don't include the M)
                if current_chunk:
                    chunks.append(current_chunk)
                    current_chunk = []
                # Note: We don't add 'M' to any chunk as it's just a marker
            else:
                # Regular command - add to current chunk
                current_chunk.append(cmd)
        
        # Add final chunk if it has commands
        if current_chunk:
            chunks.append(current_chunk)
        
        return chunks
    
    def _perform_vision_detection_sequence(self):
        """Perform the vision detection sequence (5-6 images)"""
        logger.info("Starting vision detection sequence...")
        
        if not self.client:
            logger.warning("Vision client not available")
            if self.bluetooth_conn:
                self._send_to_android({'type': 'vision_error', 'message': 'Vision system not available'})
            return
        
        try:
            # Notify Android that vision detection is starting
            if self.bluetooth_conn:
                self._send_to_android({'type': 'vision_detection_start'})
            
            # Capture 5-6 images and send each for detection
            num_images = 5
            detected_letters = []
            
            for i in range(num_images):
                logger.info("Capturing image {}/{}...".format(i + 1, num_images))
                
                # Capture single image and get detection result
                detected_letter = self.client.capture_and_detect_single()
                
                if detected_letter and detected_letter not in ("", "NO_TEXT", "DECODE_ERROR", "NA"):
                    logger.info("Image {} detected letter: {}".format(i + 1, detected_letter))
                    detected_letters.append(detected_letter)
                    
                    # Send each detection to Android immediately
                    if self.bluetooth_conn:
                        status_message = "STATUS,Image {}: Letter detected: {}\n".format(i + 1, detected_letter)
                        try:
                            self.bluetooth_conn.write(status_message.encode('utf-8'))
                            self.bluetooth_conn.flush()
                            logger.info("Sent detection result to Android: {}".format(repr(status_message)))
                        except Exception as e:
                            logger.error("Error sending detection to Android: {}".format(e))
                else:
                    logger.info("Image {} - no letter detected".format(i + 1))
                
                # Small delay between captures
                time.sleep(0.5)
            
            # Summary of all detections
            if detected_letters:
                summary = "Vision sequence complete. Detected: {}".format(", ".join(detected_letters))
                logger.info(summary)
                if self.bluetooth_conn:
                    self._send_to_android({'type': 'vision_summary', 'letters': detected_letters, 'message': summary})
            else:
                logger.info("Vision sequence complete. No letters detected.")
                if self.bluetooth_conn:
                    self._send_to_android({'type': 'vision_summary', 'letters': [], 'message': 'No letters detected'})
            
        except Exception as e:
            logger.error("Error in vision detection sequence: {}".format(e))
            if self.bluetooth_conn:
                self._send_to_android({'type': 'vision_error', 'message': str(e)})

    def _handle_obstacle_data(self, data: Dict):
        """Handle obstacle data from Android and send to algorithm backend (Legacy JSON format)"""
        try:
            obstacles = data.get('obstacles', [])
            robot_x = data.get('robot_x', 1)
            robot_y = data.get('robot_y', 1)
            robot_dir = data.get('robot_dir', 0)  # 0=North, 1=East, 2=South, 3=West
            retrying = data.get('retrying', False)
            
            logger.info("Processing {} obstacles".format(len(obstacles)))
            logger.info("Robot position: ({}, {}), direction: {}".format(robot_x, robot_y, robot_dir))
            
            # Prepare payload for algorithm backend (based on main.py)
            payload = {
                'obstacles': obstacles,
                'robot_x': robot_x,
                'robot_y': robot_y,
                'robot_dir': robot_dir,
                'retrying': retrying
            }
            
            self._send_to_algorithm_backend(payload)
                
        except Exception as e:
            logger.error("Error handling obstacle data: {}".format(e))
            self._send_to_android({'type': 'error', 'message': str(e)})
    
    def _execute_commands(self, commands: List[str]):
        """Execute movement commands on STM32 (based on working code)"""
        logger.info("Starting command execution on STM32...")
        
        # Convert high-level commands to STM32 format
        stm32_commands = self._convert_to_stm32_commands(commands)
        
        for i, cmd in enumerate(stm32_commands):
            try:
                # Send command to STM32
                self.stm32_serial.write(cmd.encode())
                logger.info("[{}/{}] Sent to STM32: {}".format(i+1, len(stm32_commands), cmd.strip()))
                
                # Update Android with progress
                progress = {'type': 'progress', 'current': i+1, 'total': len(stm32_commands)}
                self._send_to_android(progress)
                
                time.sleep(0.1)  # Based on working code timing
                
            except Exception as e:
                logger.error("Error sending command to STM32: {}".format(e))
                self._send_to_android({'type': 'error', 'message': 'STM32 communication error: {}'.format(str(e))})
                break
        
        # Wait for STM32 responses
        time.sleep(2)
        self._read_stm32_responses()
        
        # Notify completion
        self._send_to_android({'type': 'mission_complete'})
        logger.info("Mission execution completed")
    
    def _convert_to_stm32_commands(self, high_level_commands: List[str]) -> List[str]:
        """Convert high-level commands to STM32 format"""
        stm32_commands = []
        
        for cmd in high_level_commands:
            if cmd.startswith('FW'):  # Forward
                distance = int(cmd[2:])
                stm32_commands.append("T50|0|{}\n".format(distance))
            elif cmd.startswith('BW'):  # Backward
                distance = int(cmd[2:])
                stm32_commands.append("T-50|0|{}\n".format(distance))
            elif cmd.startswith('FL'):  # Forward Left
                stm32_commands.append("T50|-25|60\n")
            elif cmd.startswith('FR'):  # Forward Right
                stm32_commands.append("T50|25|85\n")
            elif cmd.startswith('BL'):  # Backward Left
                stm32_commands.append("T-50|-25|60\n")
            elif cmd.startswith('BR'):  # Backward Right
                stm32_commands.append("T-50|25|85\n")
            elif cmd.startswith('TL'):  # Turn Left
                stm32_commands.append("t50|0|15\n")
            elif cmd.startswith('TR'):  # Turn Right
                stm32_commands.append("t-50|0|15\n")
            elif cmd.startswith('SNAP'):  # Take photo
                stm32_commands.append("M\n")  # Marker/photo command
            elif cmd.startswith('FIN'):  # Finish
                stm32_commands.append("S\n")  # Stop command
            else:
                logger.warning("Unknown command: {}".format(cmd))
                stm32_commands.append("T0|0|0\n")  # Stop/pause
        
        return stm32_commands
    
    def _read_stm32_responses(self):
        """Read responses from STM32 (based on working code)"""
        logger.info("Reading STM32 responses...")
        responses = []
        timeout = time.time() + 5  # 5 second timeout
        
        while time.time() < timeout:
            try:
                if self.stm32_serial.in_waiting:
                    response = self.stm32_serial.readline().decode('utf-8').strip()
                    if response:
                        logger.info("STM32 Response: {}".format(response))
                        responses.append(response)
                        # STM32 responses are kept local, not forwarded to Android
                else:
                    time.sleep(0.1)
            except Exception as e:
                logger.error("Error reading STM32 response: {}".format(e))
                break
        
        if not responses:
            logger.warning("No responses received from STM32")
        else:
            logger.info("Total STM32 responses: {}".format(len(responses)))
    
    def _send_to_android(self, data: Dict):
        """Send data back to Android via Bluetooth"""
        if self.bluetooth_conn:
            try:
                message = json.dumps(data) + '\n'
                self.bluetooth_conn.write(message.encode('utf-8'))
                self.bluetooth_conn.flush()
                logger.debug("Sent to Android: {}".format(data))
            except Exception as e:
                logger.error("Error sending to Android: {}".format(e))
    
    def _handle_mission_start(self, data: Dict):
        """Handle mission start command"""
        self.mission_active = True
        logger.info("Mission started")
        self._send_to_android({'type': 'mission_started'})
    
    def _handle_emergency_stop(self):
        """Handle emergency stop"""
        logger.warning("Emergency stop activated!")
        self.mission_active = False
        
        # Send stop command to STM32
        if self.stm32_serial:
            try:
                self.stm32_serial.write("S\n".encode())
                logger.info("Stop command sent to STM32")
            except Exception as e:
                logger.error("Error sending stop command: {}".format(e))
        
        self._send_to_android({'type': 'emergency_stopped'})
    
    def _handle_start_vision(self):
        """Handle start vision command from Android"""
        if self.start_vision_detection():
            self._send_to_android({'type': 'vision_started'})
        else:
            self._send_to_android({'type': 'vision_start_failed'})
    
    def _handle_stop_vision(self):
        """Handle stop vision command from Android"""
        self.stop_vision_detection()
        self._send_to_android({'type': 'vision_stopped'})
    
    def cleanup(self):
        """Clean up all connections"""
        logger.info("Cleaning up connections...")
        
        self.running = False
        
        if self.stm32_serial:
            try:
                self.stm32_serial.write("S\n".encode())  # Stop command
                time.sleep(0.5)
                self.stm32_serial.close()
                logger.info("STM32 connection closed")
            except Exception as e:
                logger.error("Error closing STM32: {}".format(e))
        
        if self.bluetooth_conn:
            try:
                self.bluetooth_conn.close()
                logger.info("Bluetooth connection closed")
            except Exception as e:
                logger.error("Error closing Bluetooth: {}".format(e))
        
        # Clean up vision system
        if self.client:
            self.client.cleanup()
            logger.info("Vision system cleaned up")

def main():
    """Main function to run the communication handler"""
    handler = MDPCommunicationHandler()
    
    try:
        handler.initialize()
        logger.info("Starting communication handler...")
        logger.info("Make sure to run 'sudo rfcomm listen /dev/rfcomm0 1' first")
        
        # Start the main communication loop
        handler.start_bluetooth_listener()
        
    except KeyboardInterrupt:
        logger.info("Shutting down...")
    except Exception as e:
        logger.error("Fatal error: {}".format(e))
    finally:
        handler.cleanup()

if __name__ == "__main__":
    main()