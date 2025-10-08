#!/usr/bin/env python3
"""
Vision Client for MDP Robot System
Uses the exact same logic as working pi_client.py but adds Bluetooth integration
"""

import socket
import struct
import time
import cv2
import logging
from typing import Optional, Callable

# Try to import Pi camera modules
try:
    from picamera import PiCamera
    from picamera.array import PiRGBArray
    PI_CAMERA_AVAILABLE = True
except ImportError:
    PI_CAMERA_AVAILABLE = False
    logging.warning("Pi camera modules not available, using fallback")

logger = logging.getLogger(__name__)

class VisionClient:
    def __init__(self, server_ip='192.168.12.31', server_port=8000, callback=None):
        """
        Initialize vision client
        
        Args:
            server_ip: IP of laptop running model server
            server_port: Port of model server
            callback: Function to call when letter is detected (callback(letter))
        """
        self.server_ip = server_ip
        self.server_port = server_port
        self.callback = callback
        self.running = False

    def recv_all(self, sock, count):
        """Receive exact number of bytes from socket - same as working pi_client.py"""
        buf = b''
        while len(buf) < count:
            chunk = sock.recv(count - len(buf))
            if not chunk:
                return None
            buf += chunk
        return buf

    def start_continuous_detection(self):
        """Start detection using exact same logic as working pi_client.py"""
        if not PI_CAMERA_AVAILABLE:
            logger.error("Pi camera not available!")
            return False

        # Retry connection logic
        max_retries = 3
        retry_count = 0
        
        while retry_count < max_retries:
            try:
                # Exact same setup as working pi_client.py - ONE connection for all frames
                logger.info("Connecting to server {} {} (attempt {}/{})".format(
                    self.server_ip, self.server_port, retry_count + 1, max_retries))
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(10)
                s.connect((self.server_ip, self.server_port))
                s.settimeout(None)
                logger.info("Connected!")

                # Setup camera - exact same as working pi_client.py
                logger.info("Setting up camera...")
                camera = PiCamera()
                camera.resolution = (640, 480)
                camera.framerate = 10
                rawCapture = PiRGBArray(camera, size=(640, 480))
                
                # Give camera more time to initialize and clear any buffer issues
                time.sleep(2)
                logger.info("Camera ready!")

                self.running = True
                logger.info("Starting capture. Press Ctrl+C to stop.")
                
                # Clear any existing captures before starting fresh
                rawCapture.truncate(0)
                
                # Exact same capture loop as working pi_client.py - use SAME socket for all frames
                frame_count = 0
                for frame in camera.capture_continuous(rawCapture, format="bgr", use_video_port=True):
                    if not self.running:
                        break
                    
                    frame_count += 1
                    try:
                        image = frame.array

                        # encode to JPEG - exact same as working pi_client.py
                        ret, buf = cv2.imencode('.jpg', image, [int(cv2.IMWRITE_JPEG_QUALITY), 80])
                        if not ret:
                            logger.error("Failed to encode frame {}".format(frame_count))
                            rawCapture.truncate(0)
                            continue
                            
                        img_bytes = buf.tobytes()
                        
                        # send length prefix + image - exact same as working pi_client.py
                        s.sendall(struct.pack('>I', len(img_bytes)) + img_bytes)

                        # wait for response - exact same as working pi_client.py
                        raw_len = self.recv_all(s, 4)
                        if not raw_len:
                            logger.error("Server closed")
                            break
                            
                        resp_len = struct.unpack('>I', raw_len)[0]
                        resp = self.recv_all(s, resp_len)
                        if resp is None:
                            logger.error("Incomplete response")
                            break
                            
                        text = resp.decode('utf-8', errors='ignore')
                        logger.info("[SERVER] -> {}".format(repr(text)))

                        # NEW: Add Bluetooth sending logic here - filter out non-meaningful responses
                        if text and text not in ("", "NO_TEXT", "DECODE_ERROR", "NA"):
                            logger.info("Letter detected: {}".format(text))
                            
                            # Send to Android via callback (this is the integration part)
                            if self.callback:
                                try:
                                    self.callback(text)
                                    logger.info("Successfully sent '{}' to Android via callback".format(text))
                                except Exception as e:
                                    logger.error("Error in detection callback: {}".format(e))
                        else:
                            # Only log NA messages at debug level to reduce noise
                            if text == "NA":
                                logger.debug("[SERVER] -> 'NA' (filtered out)")
                            else:
                                logger.debug("Filtered out empty/invalid response: {}".format(repr(text)))

                        # Clear buffer - exact same as working pi_client.py
                        rawCapture.truncate(0)
                        time.sleep(0.1)  # Small throttle for stability and to prevent overwhelming server
                        
                    except Exception as frame_error:
                        logger.error("Error processing frame {}: {}".format(frame_count, frame_error))
                        # Clear buffer and continue with next frame
                        rawCapture.truncate(0)
                        continue

                # If we get here, the loop ended normally
                break

            except Exception as e:
                logger.error("Error on attempt {}/{}: {}".format(retry_count + 1, max_retries, e))
                retry_count += 1
                if retry_count < max_retries:
                    logger.info("Retrying in 2 seconds...")
                    time.sleep(2)
                else:
                    logger.error("Failed to establish stable connection after {} attempts".format(max_retries))
                    return False
            finally:
                try:
                    s.close()
                except:
                    pass
                try:
                    camera.close()
                except:
                    pass
                cv2.destroyAllWindows()
            
        return True

    def stop_detection(self):
        """Stop continuous detection"""
        self.running = False
        logger.info("Stopping vision detection...")
    
    def capture_and_detect_single(self):
        """Capture a single image and get detection result"""
        if not PI_CAMERA_AVAILABLE:
            logger.error("Pi camera not available!")
            return None
        
        try:
            logger.info("Capturing single image for detection...")
            
            # Connect to server for single detection
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(10)
            s.connect((self.server_ip, self.server_port))
            s.settimeout(None)
            
            # Setup camera for single capture
            camera = PiCamera()
            camera.resolution = (640, 480)
            rawCapture = PiRGBArray(camera, size=(640, 480))
            
            # Give camera time to initialize
            time.sleep(1)
            
            # Capture single frame
            camera.capture(rawCapture, format="bgr")
            image = rawCapture.array
            
            # Encode to JPEG
            ret, buf = cv2.imencode('.jpg', image, [int(cv2.IMWRITE_JPEG_QUALITY), 80])
            if not ret:
                logger.error("Failed to encode image")
                return None
                
            img_bytes = buf.tobytes()
            
            # Send length prefix + image
            s.sendall(struct.pack('>I', len(img_bytes)) + img_bytes)
            
            # Wait for response
            raw_len = self.recv_all(s, 4)
            if not raw_len:
                logger.error("Server closed")
                return None
                
            resp_len = struct.unpack('>I', raw_len)[0]
            resp = self.recv_all(s, resp_len)
            if resp is None:
                logger.error("Incomplete response")
                return None
                
            text = resp.decode('utf-8', errors='ignore')
            logger.info("Single detection result: {}".format(repr(text)))
            
            # Clean up
            camera.close()
            s.close()
            
            return text
            
        except Exception as e:
            logger.error("Error in single image detection: {}".format(e))
            return None
        finally:
            try:
                camera.close()
            except:
                pass
            try:
                s.close()
            except:
                pass
    
    def cleanup(self):
        """Clean up resources"""
        self.stop_detection()
        logger.info("Vision client cleaned up")

# Test function
def test_vision_client():
    """Test the vision client independently"""
    def on_letter_detected(letter):
        print("DETECTED LETTER: {}".format(letter))
    
    client = VisionClient(callback=on_letter_detected)
    
    try:
        client.start_continuous_detection()
    except KeyboardInterrupt:
        print("\n[!] Stopping (user interrupt)")
    finally:
        client.cleanup()

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
    test_vision_client()