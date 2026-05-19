#pragma once

#include <QString>

// Abstract interface for liquid lens voltage controllers.
// Concrete implementations: MockLiquidLensController (simulation),
// and a hardware-specific class per controller brand (Optotune, Varioptic, etc.)
class ILiquidLensController
{
public:
    virtual ~ILiquidLensController() = default;

    virtual bool    connect()    = 0;
    virtual void    disconnect() = 0;
    virtual bool    isConnected() const = 0;
    virtual QString deviceName()  const = 0;

    // Set the lens to the given dioptre value (or normalised voltage 0.0–1.0).
    // Units depend on the hardware — document in the concrete class.
    virtual bool    setDioptre(double dioptre) = 0;

    virtual double  minDioptre() const = 0;
    virtual double  maxDioptre() const = 0;
};
