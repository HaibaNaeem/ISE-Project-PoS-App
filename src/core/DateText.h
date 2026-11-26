#pragma once
#include <string>

std::string todayText();                                         // "2026-11-24"
std::string nowTimeText();                                       // "14:05:09"
int daysBetween(const std::string& from, const std::string& to); // to - from, in days
std::string addDays(const std::string& date, int n);
