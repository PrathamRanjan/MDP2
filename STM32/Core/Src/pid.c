#include "pid.h"

/*void pid_reset(PidDef *def) {
	def->errorArea = 0;
	def->errorOld = 0;
}

void pid_init(PidDef *def, float Kp, float Ki, float Kd) {
	pid_reset(def);

	def->Kp = Kp;
	def->Ki = Ki;
	def->Kd = Kd;
}

float pid_adjust(PidDef *def, float error, float scale) {
	def->errorArea += error;
	float errorRate = (error - def->errorOld);
	def->errorOld = error;

	return error * def->Kp * scale + def->errorArea * def->Ki * scale + errorRate * def->Kd * scale;
}*/

void pid_reset(PidDef *d) {
    d->errorArea = 0.0f;
    d->errorOld  = 0.0f;
    d->dPrev     = 0.0f;
}

void pid_init(PidDef *d, float Kp, float Ki, float Kd) {
    pid_reset(d);
    d->Kp = Kp; d->Ki = Ki; d->Kd = Kd;
    d->iMin = -1e6f; d->iMax = 1e6f;   // set sensible bounds later
    d->dAlpha = 1.0f;                  // 1.0 = no filtering; try 0.2..0.5
}

float pid_adjust(PidDef *d, float error, float dt) {
    if (dt <= 0.0f) dt = 1.0f;

    // I term with anti-windup
    d->errorArea += error * dt;
    if (d->errorArea > d->iMax) d->errorArea = d->iMax;
    if (d->errorArea < d->iMin) d->errorArea = d->iMin;

    // D term (time-scaled + optional LPF)
    float deriv = (error - d->errorOld) / dt;
    d->dPrev = d->dPrev + d->dAlpha * (deriv - d->dPrev);

    d->errorOld = error;
    return d->Kp*error + d->Ki*d->errorArea + d->Kd*d->dPrev;
}
